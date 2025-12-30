#include "jni.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_mmap.h>
#include <shmipc/jni/lib_shm_ipc_jni_IpcChannel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define CLASS_LONG_RESULT_PATH "lib/shm/ipc/result/IpcLongResult"
#define CLASS_ERROR_PATH "lib/shm/ipc/result/IpcError"
#define CLASS_ERROR_CODE_PATH CLASS_ERROR_PATH "$ErrorCode"

#define DBG(fmt, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, "[jni:init] " fmt "\n", ##__VA_ARGS__);                    \
    fflush(stderr);                                                            \
  } while (0)

static IpcChannel *get_channel(JNIEnv *env, jobject obj);
static jobject create_error_long_response(JNIEnv *env, IpcStatus status_code,
                                          const char *msg);

static jobject create_error(JNIEnv *env, IpcStatus status_code,
                            const char *msg);

// TODO: cache classes
// TODO: handle null results
// TODO: dont forget release objects

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

JNIEXPORT jobject JNICALL Java_lib_shm_ipc_jni_IpcChannel_init(
    JNIEnv *env, jobject obj, jstring path_to_file, jlong size,
    jboolean create) {
  if (path_to_file == NULL) {
    return create_error_long_response(env, IPC_ERR_INVALID_ARGUMENT,
                                      "Path to file is empty");
  }

  if (size <= 0) {
    return create_error_long_response(env, IPC_ERR_INVALID_ARGUMENT,
                                      "Illegal size");
  }

  const char *path = (*env)->GetStringUTFChars(env, path_to_file, NULL);
  if (path == NULL) {
    return create_error_long_response(env, IPC_ERR_SYSTEM, "Failed path parse");
  }

  // TODO: check if size correct
  const IpcMemorySegmentResult mmap_result = ipc_mmap(path, size);
  (*env)->ReleaseStringUTFChars(env, path_to_file, path);

  if (IpcMemorySegmentResult_is_error(mmap_result)) {
    return create_error_long_response(env, mmap_result.ipc_status,
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
      return create_error_long_response(env, create_result.ipc_status,
                                        create_result.error.detail);
    }
  } else {
    const IpcChannelConnectResult connect_result =
        ipc_channel_connect(seg->memory);
    if (IpcChannelConnectResult_is_ok(connect_result)) {
      ch = connect_result.result;
    } else {
      return create_error_long_response(env, connect_result.ipc_status,
                                        connect_result.error.detail);
    }
  }

  // if success -> need to save channel address;
  // then get address of byte

  return (jlong)(intptr_t)ch;
}

JNIEXPORT void JNICALL Java_lib_shm_ipc_jni_IpcChannel_write(JNIEnv *env,
                                                             jobject obj,
                                                             jbyteArray arr) {
  const size_t arr_len = (size_t)(*env)->GetArrayLength(env, arr);

  jboolean isCopy = JNI_FALSE;
  jbyte *bytes = (*env)->GetByteArrayElements(env, arr, &isCopy);

  IpcChannel *channel = get_channel(env, obj);
  if (!channel) {
    (*env)->ReleaseByteArrayElements(env, arr, bytes, JNI_ABORT);
    return;
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
}

JNIEXPORT jbyteArray JNICALL Java_lib_shm_ipc_jni_IpcChannel_read(JNIEnv *env,
                                                                  jobject obj) {
  IpcChannel *channel = get_channel(env, obj);
  if (!channel) {
    return NULL;
  }

  IpcEntry entry;
  struct timespec timeout = {.tv_nsec = 5000};
  IpcChannelReadResult read_result =
      ipc_channel_read(channel, &entry, &timeout);
  while (IpcChannelReadResult_is_error(read_result) &&
         read_result.ipc_status == IPC_ERR_TIMEOUT) {
    DBG("ipc_channel_read failed: status=%d, detail=%s",
        (int)read_result.ipc_status,
        read_result.error.detail ? read_result.error.detail : "unknown");
    read_result = ipc_channel_read(channel, &entry, &timeout);
  }

  const jsize len = (jsize)entry.size;
  if (len <= 0) {
    return NULL;
  }

  jbyteArray arr = (*env)->NewByteArray(env, len);
  if (!arr) {
    return NULL;
  }

  (*env)->SetByteArrayRegion(env, arr, 0, len, (const jbyte *)entry.payload);

  return arr;
}

JNIEXPORT void JNICALL Java_lib_shm_ipc_jni_IpcChannel_close(JNIEnv *, jobject);

static jobject create_error_long_response(JNIEnv *env, IpcStatus status_code,
                                          const char *msg) {
  const jclass long_result_class =
      (*env)->FindClass(env, CLASS_LONG_RESULT_PATH);
  const jmethodID error_method = (*env)->GetStaticMethodID(
      env, long_result_class, "error",
      "(Llib/shm/ipc/result/IpcError;)Llib/shm/ipc/result/IpcLongResult;");

  return (*env)->CallStaticObjectMethod(env, long_result_class, error_method,
                                        create_error(env, status_code, msg));
}

static jobject create_error(JNIEnv *env, IpcStatus status_code,
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

  const jmethodID error_constructor =
      (*env)->GetMethodID(env, error_class, "<init>",
                          "(Llib/shm/ipc/result/IpcError$ErrorCode;Ljava/lang/"
                          "String;)V");
  // TODO: handle null

  jstring error_message = (*env)->NewStringUTF(env, msg);
  // TODO: handle null

  return (*env)->NewObject(env, error_class, error_constructor,
                           error_code_instance, error_message);
}

static IpcChannel *get_channel(JNIEnv *env, jobject obj) {
  const jclass cls = (*env)->GetObjectClass(env, obj);
  const jfieldID fid_addr = (*env)->GetFieldID(env, cls, "channelAddress", "J");

  const uint64_t addr = (uint64_t)(*env)->GetLongField(env, obj, fid_addr);
  (*env)->DeleteLocalRef(env, cls);

  return (IpcChannel *)(void *)addr;
}
