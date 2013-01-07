#include "server-jni.h"

#include <string.h>

struct {
    jclass class;
    jfieldID listener_ptr;
    jmethodID onNotify;
} Listener;

void Java_org_freedesktop_wayland_server_Listener_detach(JNIEnv * env,
        jobject jlistener);

struct wl_jni_listener *
wl_jni_listener_from_java(JNIEnv * env, jobject jlistener)
{
    if (jlistener == NULL)
        return NULL;

    return (struct wl_jni_listener *)(intptr_t)
            (*env)->GetLongField(env, jlistener, Listener.listener_ptr);
}

void
wl_jni_listener_added_to_signal(JNIEnv * env, jobject jlistener)
{
    struct wl_jni_listener * jni_listener;

    jni_listener = wl_jni_listener_from_java(env, jlistener);

    if (jni_listener == NULL)
        return;

    jni_listener->self_ref = (*env)->NewGlobalRef(env, jlistener);
}

static void
listener_notify_func(struct wl_listener * listener, void * data)
{
    struct wl_jni_listener * jni_listener;
    JNIEnv * env;

    jni_listener = wl_container_of(listener, jni_listener, listener);

    env = wl_jni_get_env();

    /* TODO: Do something with the data parameter? */
    (*env)->CallVoidMethod(env, jni_listener->self_ref, Listener.onNotify);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
        /* TODO */
    }
}

static void
listener_destroy_func(struct wl_listener * listener, void * data)
{
    struct wl_jni_listener * jni_listener;
    JNIEnv * env;

    jni_listener = wl_container_of(listener, jni_listener, listener);

    env = wl_jni_get_env();

    Java_org_freedesktop_wayland_server_Listener_detach(
            env, jni_listener->self_ref);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
        /* TODO */
    }
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Listener__1create(JNIEnv * env,
        jobject jlistener)
{
    struct wl_jni_listener * jni_listener;

    jni_listener = malloc(sizeof(struct wl_jni_listener));
    if (jni_listener == NULL) {
        wl_jni_throw_OutOfMemoryError(env, NULL);
        return;
    }

    memset(jni_listener, 0, sizeof(struct wl_jni_listener));

    // TODO: Set callbacks

    (*env)->SetLongField(env, jlistener, Listener.listener_ptr,
            (long)(intptr_t)jni_listener);
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Listener_detach(JNIEnv * env,
        jobject jlistener)
{
    struct wl_jni_listener * jni_listener;
    
    jni_listener = wl_jni_listener_from_java(env, jlistener);
    if (jni_listener == NULL)
        return; /* This is an error */

    wl_list_remove(&jni_listener->listener.link);
    wl_list_remove(&jni_listener->destroy_listener.link);

    (*env)->DeleteGlobalRef(env, jni_listener->self_ref);
    jni_listener->self_ref = NULL;
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Listener__1destroy(JNIEnv * env,
        jobject jlistener)
{
    struct wl_jni_listener * jni_listener;
    
    jni_listener = wl_jni_listener_from_java(env, jlistener);
    if (jni_listener == NULL)
        return;

    free(jni_listener);
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Listener_initializeJNI(JNIEnv * env,
        jclass cls)
{
    Listener.class = (*env)->NewGlobalRef(env, cls);
    if (Listener.class == NULL)
        return; /* Exception Thrown */
        
    Listener.listener_ptr = (*env)->GetFieldID(env, Listener.class,
            "listener_ptr", "J");
    if (Listener.listener_ptr == NULL)
        return; /* Exception Thrown */

    Listener.onNotify = (*env)->GetMethodID(env, Listener.class,
            "onNotify", "()V");
    if (Listener.onNotify == NULL)
        return; /* Exception Thrown */
}
