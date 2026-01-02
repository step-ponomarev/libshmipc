#include "jni.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_mmap.h>
#include <shmipc/jni/lib_shm_ipc_jni_IpcChannel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define CLASS_IPC_CHANNEL_PATH "lib/shm/ipc/jni/IpcChannel"
#define CLASS_LONG_RESULT_PATH "lib/shm/ipc/result/IpcLongResult"
#define CLASS_BYTES_RESULT_PATH "lib/shm/ipc/result/IpcBytesResult"
#define CLASS_BOOLEAN_RESULT_PATH "lib/shm/ipc/result/IpcBooleanResult"
#define CLASS_IPC_CHANNEL_RESULT_PATH "lib/shm/ipc/result/IpcChannelResult"
#define CLASS_ERROR_PATH "lib/shm/ipc/result/IpcError"
#define CLASS_ERROR_CODE_PATH CLASS_ERROR_PATH "$ErrorCode"

#define DBG(fmt, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, "[jni:init] " fmt "\n", ##__VA_ARGS__);                    \
    fflush(stderr);                                                            \
  } while (0)

static jobject _init(JNIEnv *env, jstring path_to_file, jlong size,
                     jboolean create);

static IpcChannel *_get_channel(JNIEnv *env, jobject obj);
static jobject _create_ipc_channel(JNIEnv *env, uint64_t channel_address,
                                   uint64_t polling_address);
static jobject _create_error_long_result(JNIEnv *env, IpcStatus status_code,
                                         const char *msg);
static jobject _create_long_result(JNIEnv *env, uint64_t val);

static jobject _create_bytes_result(JNIEnv *env, uint8_t *bytes, size_t len);
static jobject _create_error_bytes_result(JNIEnv *env, IpcStatus status_code,
                                          const char *msg);

static jobject _create_boolean_result(JNIEnv *env, bool val);
static jobject _create_error_boolean_result(JNIEnv *env, IpcStatus status_code,
                                            const char *msg);

static jobject _create_error_ipc_channel_result(JNIEnv *env,
                                                IpcStatus status_code,
                                                const char *msg);

static jobject _create_error(JNIEnv *env, IpcStatus status_code,
                             const char *msg);

JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_create(JNIEnv *env,
                                                                 jclass class,
                                                                 jstring path,
                                                                 jlong size) {
  return _init(env, path, size, true);
}

JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_attach(JNIEnv *env,
                                                                 jclass class,
                                                                 jstring path,
                                                                 jlong size) {
  return _init(env, path, size, false);
}

/*
 * Class:     lib_shm_ipc_jni_IpcChannel
 * Method:    write
 * Signature: ([B)Llib/shm/ipc/IpcResultWrapper;
 */
JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_write(JNIEnv *,
                                                                jobject,
                                                                jbyteArray);

/*
 * Class:     lib_shm_ipc_jni_IpcChannel
 * Method:    tryRead
 * Signature: ()Llib/shm/ipc/IpcResultWrapper;
 */
JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_tryRead(JNIEnv *,
                                                                  jobject);

/*
 * Class:     lib_shm_ipc_jni_IpcChannel
 * Method:    init
 * Signature: (Ljava/lang/String;JZ)Llib/shm/ipc/IpcResultWrapper;
 */

JNIEXPORT jlong JNICALL Java_lib_shm_ipc_jni_IpcChannel_suggestSize(
    JNIEnv *___, jclass __, jlong desired_capacity) {
  return (jlong)ipc_channel_suggest_size((size_t)desired_capacity);
}
JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_write(
    JNIEnv *env, jobject obj, jbyteArray arr) {
  const size_t arr_len = (size_t)(*env)->GetArrayLength(env, arr);

  jboolean isCopy = JNI_FALSE;
  jbyte *bytes = (*env)->GetByteArrayElements(env, arr, &isCopy);

  IpcChannel *channel = _get_channel(env, obj);
  if (!channel) {
    (*env)->ReleaseByteArrayElements(env, arr, bytes, JNI_ABORT);
    return _create_error_boolean_result(env, IPC_ERR_SYSTEM,
                                        "Channel is not initialized correctly");
  }

  IpcChannelWriteResult write_result =
      ipc_channel_write(channel, (const void *)bytes, arr_len);
  while (IpcChannelWriteResult_is_error(write_result) &&
         write_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
    DBG("ipc_channel_write failed: status=%d, detail=%s",
        (int)write_result.ipc_status,
        write_result.error.detail ? write_result.error.detail : "unknown");

    write_result = ipc_channel_write(channel, (const void *)bytes, arr_len);
  }

  (*env)->ReleaseByteArrayElements(env, arr, bytes, JNI_ABORT);

  return _create_boolean_result(env, true);
}

JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_tryRead(JNIEnv *env,
                                                                  jobject obj) {
  IpcChannel *channel = _get_channel(env, obj);
  if (!channel) {
    return _create_error_bytes_result(env, IPC_ERR_SYSTEM,
                                      "Channel is not initialized correctly");
  }

  IpcEntry entry;
  IpcChannelTryReadResult res = ipc_channel_try_read(channel, &entry);
  if (IpcChannelTryReadResult_is_error(res)) {
    return _create_error_bytes_result(env, res.ipc_status, res.error.detail);
  }

  return _create_bytes_result(env, entry.payload, entry.size);
}

JNIEXPORT void JNICALL Java_lib_shm_ipc_jni_IpcChannel_close(JNIEnv *, jobject);

static jobject _init(JNIEnv *env, jstring path_to_file, jlong size,
                     jboolean create) {
  if (path_to_file == NULL) {
    return _create_error_ipc_channel_result(env, IPC_ERR_INVALID_ARGUMENT,
                                            "Path to file is empty");
  }

  if (size <= 0) {
    return _create_error_ipc_channel_result(env, IPC_ERR_INVALID_ARGUMENT,
                                            "Illegal size");
  }

  const char *path = (*env)->GetStringUTFChars(env, path_to_file, NULL);
  if (path == NULL) {
    return _create_error_ipc_channel_result(env, IPC_ERR_SYSTEM,
                                            "Failed path parse");
  }

  // TODO: check if size correct
  const IpcMemorySegmentResult mmap_result = ipc_mmap(path, size);
  (*env)->ReleaseStringUTFChars(env, path_to_file, path);

  if (IpcMemorySegmentResult_is_error(mmap_result)) {
    return _create_error_ipc_channel_result(env, mmap_result.ipc_status,
                                            mmap_result.error.detail);
  }

  const IpcMemorySegment *seg = mmap_result.result;

  IpcChannel *ch = NULL;
  if ((bool)create) {
    const IpcChannelOpenResult create_result =
        ipc_channel_create(seg->memory, seg->size);
    if (IpcChannelOpenResult_is_ok(create_result)) {
      ch = create_result.result;
    } else {
      return _create_error_ipc_channel_result(env, create_result.ipc_status,
                                              create_result.error.detail);
    }
  } else {
    const IpcChannelConnectResult connect_result =
        ipc_channel_connect(seg->memory);
    if (IpcChannelConnectResult_is_ok(connect_result)) {
      ch = connect_result.result;
    } else {
      return _create_error_ipc_channel_result(env, connect_result.ipc_status,
                                              connect_result.error.detail);
    }
  }

  return _create_ipc_channel(env, (jlong)(intptr_t)ch,
                             (jlong)(intptr_t)mmap_result.result->memory);
}

static jobject _create_bytes_result(JNIEnv *env, uint8_t *bytes, size_t len) {
  const jsize length = (jsize)len;

  jbyteArray arr = (*env)->NewByteArray(env, length);
  (*env)->SetByteArrayRegion(env, arr, 0, len, (const jbyte *)bytes);

  const jclass long_result_class =
      (*env)->FindClass(env, CLASS_BYTES_RESULT_PATH);
  const jmethodID ok_method = (*env)->GetStaticMethodID(
      env, long_result_class, "ok", "(J)Llib/shm/ipc/result/IpcLongResult;");

  return (*env)->CallStaticObjectMethod(env, long_result_class, ok_method, arr);
}

static jobject _create_error_bytes_result(JNIEnv *env, IpcStatus status_code,
                                          const char *msg) {

  const jclass result_class = (*env)->FindClass(env, CLASS_BYTES_RESULT_PATH);
  const jmethodID error_method = (*env)->GetStaticMethodID(
      env, result_class, "error",
      "(Llib/shm/ipc/result/IpcError;)Llib/shm/ipc/result/IpcChannelResult;");

  return (*env)->CallStaticObjectMethod(env, result_class, error_method,
                                        _create_error(env, status_code, msg));
}

static jobject _create_boolean_result(JNIEnv *env, bool val) {
  const jclass bytes_result_class =
      (*env)->FindClass(env, CLASS_BOOLEAN_RESULT_PATH);

  const jmethodID ok_method =
      (*env)->GetStaticMethodID(env, bytes_result_class, "ok",
                                "(Z)Llib/shm/ipc/result/IpcBooleanResult;");

  return (*env)->CallStaticObjectMethod(env, bytes_result_class, ok_method,
                                        val);
}

static jobject _create_error_boolean_result(JNIEnv *env, IpcStatus status_code,
                                            const char *msg) {
  const jclass result_class = (*env)->FindClass(env, CLASS_BOOLEAN_RESULT_PATH);
  const jmethodID error_method = (*env)->GetStaticMethodID(
      env, result_class, "error",
      "(Llib/shm/ipc/result/IpcError;)Llib/shm/ipc/result/IpcBooleanResult;");

  return (*env)->CallStaticObjectMethod(env, result_class, error_method,
                                        _create_error(env, status_code, msg));
}

static jobject _create_ipc_channel(JNIEnv *env, uint64_t channel_address,
                                   uint64_t polling_address) {
  const jclass ipc_channel_class =
      (*env)->FindClass(env, CLASS_IPC_CHANNEL_PATH);

  const jmethodID ipc_channel_constructor =
      (*env)->GetMethodID(env, ipc_channel_class, "<init>", "(JJ)V");

  return (*env)->NewObject(env, ipc_channel_class, ipc_channel_constructor,
                           channel_address, polling_address);
}

static jobject _create_long_result(JNIEnv *env, uint64_t val) {
  const jclass long_result_class =
      (*env)->FindClass(env, CLASS_LONG_RESULT_PATH);
  const jmethodID ok_method = (*env)->GetStaticMethodID(
      env, long_result_class, "ok", "(J)Llib/shm/ipc/result/IpcLongResult;");

  return (*env)->CallStaticObjectMethod(env, long_result_class, ok_method, val);
}

static jobject _create_ipc_channel_result(JNIEnv *env, uint64_t val) {
  const jclass long_result_class =
      (*env)->FindClass(env, CLASS_IPC_CHANNEL_PATH);
  const jmethodID ok_method = (*env)->GetStaticMethodID(
      env, long_result_class, "ok", "(J)Llib/shm/ipc/result/IpcLongResult;");

  return (*env)->CallStaticObjectMethod(env, long_result_class, ok_method, val);
}

static jobject _create_error_ipc_channel_result(JNIEnv *env,
                                                IpcStatus status_code,
                                                const char *msg) {
  const jclass channel_result_class =
      (*env)->FindClass(env, CLASS_IPC_CHANNEL_RESULT_PATH);
  const jmethodID error_method = (*env)->GetStaticMethodID(
      env, channel_result_class, "error",
      "(Llib/shm/ipc/result/IpcError;)Llib/shm/ipc/result/IpcChannelResult;");

  return (*env)->CallStaticObjectMethod(env, channel_result_class, error_method,
                                        _create_error(env, status_code, msg));
}

static jobject _create_error(JNIEnv *env, IpcStatus status_code,
                             const char *msg) {
  const jclass error_code_class = (*env)->FindClass(env, CLASS_ERROR_CODE_PATH);
  // TODO: handle null

  const jmethodID value_of_method =
      (*env)->GetStaticMethodID(env, error_code_class, "valueOf",
                                "(I)Llib/shm/ipc/result/IpcError$ErrorCode;");
  // TODO: handle null

  jobject error_code_instance = (*env)->CallStaticObjectMethod(
      env, error_code_class, value_of_method, (int32_t)status_code);
  // TODO: handle null

  const jclass error_class = (*env)->FindClass(env, CLASS_ERROR_PATH);
  // TODO: handle null

  const jmethodID error_constructor = (*env)->GetMethodID(
      env, error_class, "<init>",
      "(Llib/shm/ipc/result/IpcError$ErrorCode;Ljava/lang/String;)V");
  // TODO: handle null

  jstring error_message = (*env)->NewStringUTF(env, msg);
  // TODO: handle null

  return (*env)->NewObject(env, error_class, error_constructor,
                           error_code_instance, error_message);
}

static IpcChannel *_get_channel(JNIEnv *env, jobject obj) {
  const jclass cls = (*env)->GetObjectClass(env, obj);
  const jfieldID fid_addr = (*env)->GetFieldID(env, cls, "channelAddress", "J");

  const uint64_t addr = (uint64_t)(*env)->GetLongField(env, obj, fid_addr);
  (*env)->DeleteLocalRef(env, cls);

  return (IpcChannel *)(void *)addr;
}
