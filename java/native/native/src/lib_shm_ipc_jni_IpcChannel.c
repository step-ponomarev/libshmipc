#include "jni.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_mmap.h>
#include <shmipc/jni/lib_shm_ipc_jni_IpcChannel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#define DBG(fmt, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, "[jni:init] " fmt "\n", ##__VA_ARGS__);                    \
    fflush(stderr);                                                            \
  } while (0)

typedef struct IpcChannelConfigurationJni {
  bool create;
} IpcChannelConfigurationJni;

static IpcChannel *get_channel(JNIEnv *env, jobject obj);
static IpcChannelConfigurationJni parce_config(JNIEnv *env, jobject conf);

JNIEXPORT jlong JNICALL Java_lib_shm_ipc_jni_IpcChannel_init(
    JNIEnv *env, jobject obj, jstring path_to_file, jlong size,
    jboolean is_producer) {
  if (!path_to_file) {
    DBG("path_to_file=NULL");
    return 0;
  }
  if (size <= 0) {
    DBG("size<=0");
    return 0;
  }

  const char *path = (*env)->GetStringUTFChars(env, path_to_file, NULL);
  if (!path) {
    DBG("GetStringUTFChars failed (OOM/pending ex)");
    return 0;
  }

  DBG("path='%s' size=%llu", path, (unsigned long long)size);

  uint64_t aligned = ipc_channel_suggest_size((size_t)size);
  IpcMemorySegmentResult mmap_result = ipc_mmap(path, aligned);

  if (IpcMemorySegmentResult_is_error(mmap_result)) {
    DBG("ipc_mmap failed: status=%d, detail=%s", (int)mmap_result.ipc_status,
        mmap_result.error.detail ? mmap_result.error.detail : "unknown");
    (*env)->ReleaseStringUTFChars(env, path_to_file, path);
    return 0;
  }

  IpcMemorySegment *seg = mmap_result.result;
  DBG("ipc_mmap => seg=%p (mem=%p, size=%llu)", (void *)seg,
      seg ? seg->memory : NULL, seg ? (unsigned long long)seg->size : 0ULL);

  (*env)->ReleaseStringUTFChars(env, path_to_file, path);

  if (!seg || !seg->memory || seg->size == 0) {
    DBG("mmap failed or invalid segment");
    return 0;
  }

  IpcChannel *ch = NULL;
  if ((bool)is_producer) {
    IpcChannelOpenResult create_result =
        ipc_channel_create(seg->memory, seg->size);
    if (IpcChannelOpenResult_is_ok(create_result)) {
      ch = create_result.result;
    } else {
      DBG("ipc_channel_create failed: status=%d, detail=%s",
          (int)create_result.ipc_status,
          create_result.error.detail ? create_result.error.detail : "unknown");
      return 0;
    }
  } else {
    IpcChannelConnectResult connect_result = ipc_channel_connect(seg->memory);
    if (IpcChannelConnectResult_is_ok(connect_result)) {
      ch = connect_result.result;
    } else {
      DBG("ipc_channel_connect failed: status=%d, detail=%s",
          (int)connect_result.ipc_status,
          connect_result.error.detail ? connect_result.error.detail
                                      : "unknown");
      return 0;
    }
  }

  DBG("channel=%p", (void *)ch);

  if (!ch) {
    DBG("ipc_channel_%s returned NULL",
        (bool)is_producer ? "create" : "connect");
    return 0;
  }

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
  if (IpcChannelWriteResult_is_error(write_result)) {
    DBG("ipc_channel_write failed: status=%d, detail=%s",
        (int)write_result.ipc_status,
        write_result.error.detail ? write_result.error.detail : "unknown");
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
  struct timespec timeout = {.tv_sec = 2};
  IpcChannelReadResult read_result =
      ipc_channel_read(channel, &entry, &timeout);

  if (IpcChannelReadResult_is_error(read_result)) {
    DBG("ipc_channel_read failed: status=%d, detail=%s",
        (int)read_result.ipc_status,
        read_result.error.detail ? read_result.error.detail : "unknown");
    return NULL;
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

static IpcChannel *get_channel(JNIEnv *env, jobject obj) {
  const jclass cls = (*env)->GetObjectClass(env, obj);
  const jfieldID fid_addr = (*env)->GetFieldID(env, cls, "channelAddress", "J");

  const uint64_t addr = (uint64_t)(*env)->GetLongField(env, obj, fid_addr);
  (*env)->DeleteLocalRef(env, cls);

  return (IpcChannel *)(void *)addr;
}
