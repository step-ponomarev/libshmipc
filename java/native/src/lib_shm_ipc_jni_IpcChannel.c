#include "jni.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_mmap.h>
#include <shmipc/jni/lib_shm_ipc_jni_IpcChannel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define DBG(fmt, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, "[jni:init] " fmt "\n", ##__VA_ARGS__);                    \
    fflush(stderr);                                                            \
  } while (0)

typedef struct IpcChannelConfigurationJni {
  IpcChannelConfiguration config;
  bool create;
} IpcChannelConfigurationJni;

static IpcChannel *get_channel(JNIEnv *env, jobject obj);
static IpcChannelConfigurationJni parce_config(JNIEnv *env, jobject conf);

JNIEXPORT jlong JNICALL Java_lib_shm_ipc_jni_IpcChannel_init(
    JNIEnv *env, jobject obj, jstring path_to_file, jlong size, jobject conf) {
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

  uint64_t aligned = ipc_channel_align_size((size_t)size);
  // 1) mmap
  IpcMemorySegmentResult mmap_result = ipc_mmap(path, aligned);
  
  if (IpcMemorySegmentResult_is_error(mmap_result)) {
    DBG("ipc_mmap failed: status=%d, detail=%s", 
        (int)mmap_result.ipc_status, 
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

  // 2) config
  IpcChannelConfigurationJni cfg = parce_config(env, conf);

  // 3) create/connect
  IpcChannel *ch = NULL;
  if (cfg.create) {
    IpcChannelResult create_result = ipc_channel_create(seg->memory, seg->size, cfg.config);
    if (IpcChannelResult_is_ok(create_result)) {
      ch = create_result.result;
    } else {
      DBG("ipc_channel_create failed: status=%d, detail=%s",
          (int)create_result.ipc_status,
          create_result.error.detail ? create_result.error.detail : "unknown");
      return 0;
    }
  } else {
    IpcChannelConnectResult connect_result = ipc_channel_connect(seg->memory, cfg.config);
    if (IpcChannelConnectResult_is_ok(connect_result)) {
      ch = connect_result.result;
    } else {
      DBG("ipc_channel_connect failed: status=%d, detail=%s",
          (int)connect_result.ipc_status,
          connect_result.error.detail ? connect_result.error.detail : "unknown");
      return 0;
    }
  }

  DBG("channel=%p", (void *)ch);

  if (!ch) {
    DBG("ipc_channel_%s returned NULL", cfg.create ? "create" : "connect");
    return 0;
  }

  // 4) (диагностика layout’а, если понадобится)
  // DBG("IpcEntry sizeof=%zu off(payload)=%zu off(size)=%zu", sizeof(IpcEntry),
  // offsetof(IpcEntry, payload), offsetof(IpcEntry, size));

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
  
  IpcChannelWriteResult write_result = ipc_channel_write(channel, (const void *)bytes, arr_len);
  if (IpcChannelWriteResult_is_error(write_result)) {
    DBG("ipc_channel_write failed: status=%d, detail=%s",
        (int)write_result.ipc_status,
        write_result.error.detail ? write_result.error.detail : "unknown");
  }

  (*env)->ReleaseByteArrayElements(env, arr, bytes, JNI_ABORT);
}

/*
 * Class:     lib_shm_ipc_jni_IpcChannel
 * Method:    read
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_lib_shm_ipc_jni_IpcChannel_read(JNIEnv *env,
                                                                  jobject obj) {
  IpcChannel *channel = get_channel(env, obj);
  if (!channel) {
    return NULL;
  }

  IpcEntry entry;
  IpcChannelReadResult read_result = ipc_channel_read(channel, &entry);
  
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

/*
 * Class:     lib_shm_ipc_jni_IpcChannel
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_lib_shm_ipc_jni_IpcChannel_close(JNIEnv *, jobject);

static IpcChannelConfigurationJni parce_config(JNIEnv *env, jobject conf) {
  const jclass cls = (*env)->GetObjectClass(env, conf);

  const jfieldID fid_create = (*env)->GetFieldID(env, cls, "create", "Z");
  const bool create = (bool)(*env)->GetBooleanField(env, conf, fid_create);

  return (IpcChannelConfigurationJni){.config = {.max_round_trips = 100,
                                                 .max_sleep_ns = 2000,
                                                 .start_sleep_ns = 20},
                                      .create = create};
}

static IpcChannel *get_channel(JNIEnv *env, jobject obj) {
  const jclass cls = (*env)->GetObjectClass(env, obj);
  const jfieldID fid_addr = (*env)->GetFieldID(env, cls, "channelAddress", "J");

  const uint64_t addr = (uint64_t)(*env)->GetLongField(env, obj, fid_addr);
  (*env)->DeleteLocalRef(env, cls);

  return (IpcChannel *)(void *)addr;
}