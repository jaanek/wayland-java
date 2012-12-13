#ifndef __WAYLAND_JAVA_SERVER_JNI_H__
#define __WAYLAND_JAVA_SERVER_JNI_H__

#include "wayland-jni.h"

struct wl_client * wl_jni_client_from_java(JNIEnv * env, jobject client);
jobject wl_jni_client_to_java(JNIEnv * env, struct wl_client * client);

struct wl_display * wl_jni_display_from_java(JNIEnv * env, jobject display);
jobject wl_jni_display_to_java(JNIEnv * env, struct wl_display * display);

struct wl_event_loop * wl_jni_event_loop_from_java(JNIEnv * env, jobject event_loop);
jobject wl_jni_event_loop_to_java(JNIEnv * env, struct wl_event_loop * event_loop);

struct wl_resource * wl_jni_resource_from_java(JNIEnv * env, jobject resource);
jobject wl_jni_resource_to_java(JNIEnv * env, struct wl_resource * resource);

void wl_jni_resource_call_request(struct wl_client * client,
        struct wl_resource * resource,
        const char * method_name, const char * prototype,
        const char * java_prototype, ...);

#endif /* ! defined __WAYLAND_JAVA_SERVER_JNI_H__ */
