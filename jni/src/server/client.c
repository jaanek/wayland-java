/*
 * Copyright © 2012-2013 Jason Ekstrand.
 *  
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 * 
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <wayland-server.h>

#include "server/server-jni.h"

struct {
    jclass class;
    jmethodID init_long;
    jmethodID init_display_int;
} Client;

static void ensure_client_object_cache(JNIEnv * env, jclass cls);

struct wl_client *
wl_jni_client_from_java(JNIEnv * env, jobject jclient)
{
    return (struct wl_client *)wl_jni_object_wrapper_get_data(env, jclient);
}

jobject
wl_jni_client_to_java(JNIEnv * env, struct wl_client * client)
{
    jobject jclient;

    jclient = wl_jni_object_wrapper_get_java_from_data(env, client);
    if (jclient != NULL)
        return jclient;
    
    /*
     * If we get to this point, we've never seen the client so we need to make
     * a new wrapper object.
     */

    ensure_client_object_cache(env, NULL);

    return (*env)->NewObject(env, Client.class, Client.init_long,
            (jlong)(intptr_t)client);
}

JNIEXPORT jobject JNICALL
Java_org_freedesktop_wayland_server_Client_startClient(JNIEnv * env,
        jclass cls, jobject jdisplay, jobject jfile, jarray jargs)
{
    jmethodID mid;
    jstring jexec_path, jarg;
    jobject jclient;
    int nargs, arg;
    char * exec_path;
    char fd_str[16];
    char ** args;
    pid_t pid;
    int sockets[2];
    int flags;

    jclient = NULL;

    /* Make sure that Client is properly loaded */
    ensure_client_object_cache(env, cls);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE)
        return NULL;
    
    /* Get the string for the executable path */
    cls = (*env)->GetObjectClass(env, jfile);
    if (cls == NULL) return NULL; /* Exception Thrown */
    mid = (*env)->GetMethodID(env, cls, "getPath", "()Ljava/lang/String;");
    if (mid == NULL) return NULL; /* Exception Thrown */
    jexec_path = (*env)->CallObjectMethod(env, jfile, mid);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) return NULL;
    exec_path = wl_jni_string_to_default(env, jexec_path);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) return NULL;

    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, jexec_path);

    nargs = (*env)->GetArrayLength(env, jargs);
    args = malloc((nargs + 1) * sizeof(char *));
    if (args == NULL) {
        free(exec_path);
        wl_jni_throw_OutOfMemoryError(env, NULL);
        return NULL;
    }

    args[nargs] = NULL;
    for (arg = 0; arg < nargs; ++arg) {
        jarg = (*env)->GetObjectArrayElement(env, jargs, arg);
        if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
            --arg;
            goto cleanup_arguments;
        }

        args[arg] = wl_jni_string_to_utf8(env, jarg);
        if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
            goto cleanup_arguments;
        }
        (*env)->DeleteLocalRef(env, jarg);
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) {
        (*env)->ThrowNew(env,
            (*env)->FindClass(env, "java/lang/io/IOException"),
            "socketpair failed");
        goto cleanup_arguments;
    }

    pid = fork();
    if (pid == -1) {
        (*env)->ThrowNew(env,
                (*env)->FindClass(env, "java/lang/io/IOException"),
                "Fork Failed");
    } else if (pid == 0) {
        // We are the child
        
        // Close the parent socket
        close(sockets[0]);

        snprintf(fd_str, 16, "%d", sockets[1]);
        setenv("WAYLAND_SOCKET", fd_str, 1);
        setenv("XDG_RUNTIME_DIR", "/data/data/net.jlekstrand.wayland/", 1);

        execv(exec_path, args);
        // This is bad
        exit(-1);
    } else {
        close(sockets[1]);
        flags = fcntl(sockets[0], F_GETFD);
        flags |= FD_CLOEXEC;
        if (fcntl(sockets[0], F_SETFD, flags) == -1) {
            (*env)->ThrowNew(env,
                (*env)->FindClass(env, "java/lang/io/IOException"),
                "fnctl failed");
            goto cleanup_arguments;
        }

        jclient = (*env)->NewObject(env, Client.class, Client.init_display_int,
                jdisplay, (jint)sockets[0]);

    }

cleanup_arguments:
    for (; arg >= 0; --arg)
        free(args[arg]);
    free(args);
    free(exec_path);

    return jclient;
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Client_setNative(JNIEnv *env,
        jobject jclient, jlong client_ptr)
{
    struct wl_jni_object_wrapper *wrapper;
    struct wl_client *client;

    client = (struct wl_client *)(intptr_t)client_ptr;
    if (client_ptr == 0) {
        wl_jni_throw_IllegalArgumentException(env, "client_ptr cannot be 0");
        return;
    }

    wrapper = wl_jni_object_wrapper_set_data(env, jclient, client);
    if (wrapper == NULL)
        return; /* Exception Thrown */

    wl_jni_object_wrapper_owned(env, jclient, NULL, JNI_TRUE);
    if ((*env)->ExceptionCheck(env))
        return; /* Exception Thrown */

    wl_client_add_destroy_listener(client, &wrapper->destroy_listener);
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Client_create(JNIEnv * env, jobject jclient,
        jobject jdisplay, jint fd)
{
    struct wl_client *client;
    struct wl_display *display;
    struct wl_jni_object_wrapper *wrapper;

    display = wl_jni_display_from_java(env, jdisplay);
    if ((*env)->ExceptionCheck(env))
        return; /* Exception Thrown */

    if (display == NULL) {
        wl_jni_throw_NullPointerException(env, "Display cannot be null");
        return; /* Exception Thrown */
    }

    client = wl_client_create(display, fd);

    if (client == NULL) {
        wl_jni_throw_from_errno(env, errno);
        return;
    }

    wrapper = wl_jni_object_wrapper_set_data(env, jclient, client);
    if (wrapper == NULL) {
        wl_client_destroy(client);
        return;
    }

    wl_jni_object_wrapper_owned(env, jclient, NULL, JNI_TRUE);
    if ((*env)->ExceptionCheck(env)) {
        wl_client_destroy(client);
        return;
    }

    wl_client_add_destroy_listener(client, &wrapper->destroy_listener);
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Client_flush(JNIEnv * env, jobject jclient)
{
    wl_client_flush(wl_jni_client_from_java(env, jclient));
}

JNIEXPORT jobject JNICALL
Java_org_freedesktop_wayland_server_Client_newObject(JNIEnv * env,
        jobject jclient, jobject jiface, jobject jdata)
{
    struct wl_client * client;
    struct wl_resource * resource;
    struct wl_object tmp_obj;
    jobject jresource;

    client = wl_jni_client_from_java(env, jclient);
    if (client == NULL)
        return NULL; /* Exception Thrown */

    wl_jni_interface_init_object(env, jiface, &tmp_obj);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE)
        return NULL; /* Exception Thrown */

    resource = wl_client_new_object(client, tmp_obj.interface,
            tmp_obj.implementation, NULL);
    if (resource == NULL)
        return NULL; /* Error */

    jresource = wl_jni_resource_create_from_native(env, resource, jdata);
    if (jresource == NULL) {
        wl_resource_destroy(resource);
        return NULL; /* Exception Thrown */
    }

    return jresource;
}

JNIEXPORT jobject JNICALL
Java_org_freedesktop_wayland_server_Client_addDestroyListener(JNIEnv * env,
        jobject jclient, jobject jlistener)
{
    struct wl_client * client;
    struct wl_jni_listener * jni_listener;

    client = wl_jni_client_from_java(env, jclient);
    jni_listener = wl_jni_listener_from_java(env, jlistener);

    if (jni_listener == NULL) {
        wl_jni_throw_NullPointerException(env,
                "Listener not allowed to be null");
        return;
    }

    wl_client_add_destroy_listener(client, &jni_listener->listener);
    wl_client_add_destroy_listener(client, &jni_listener->destroy_listener);
}

JNIEXPORT jobject JNICALL
Java_org_freedesktop_wayland_server_Client_addObject(JNIEnv * env,
        jobject jclient, jobject jiface, jint id, jobject jdata)
{
    struct wl_client * client;
    struct wl_resource * resource;
    struct wl_object tmp_obj;
    jobject jresource;

    client = wl_jni_client_from_java(env, jclient);
    if (client == NULL)
        return NULL; /* Exception Thrown */

    wl_jni_interface_init_object(env, jiface, &tmp_obj);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE)
        return NULL; /* Exception Thrown */

    resource = wl_client_add_object(client, tmp_obj.interface,
            tmp_obj.implementation, (uint32_t)id, NULL);
    if (resource == NULL)
        return NULL; /* Error */

    jresource = wl_jni_resource_create_from_native(env, resource, jdata);
    if (jresource == NULL) {
        wl_resource_destroy(resource);
        return NULL; /* Exception Thrown */
    }

    return jresource;
}

JNIEXPORT jint JNICALL
Java_org_freedesktop_wayland_server_Client_addResource(JNIEnv * env,
        jobject jclient, jobject jresource)
{
    /* TODO: Handle the case of already attached resources */
    struct wl_client * client;
    struct wl_resource * resource;
    jobject global_ref;

    if (jresource == NULL) {
        wl_jni_throw_NullPointerException(env, NULL);
        return;
    }

    client = wl_jni_client_from_java(env, jclient);
    resource = wl_jni_resource_from_java(env, jresource);
    if (client == NULL || resource == NULL)
        return; /* Exception Thrown */

    /*
     * This will handle making sure garbage collection happens properly. Notice
     * that this function call is before wl_client_add_resource. This is
     * because wl_client_add_resource will modify the client parameter of the
     * resource and might break garbage collection
     */
    wl_jni_resource_set_client(env, resource, client);
    if ((*env)->ExceptionCheck(env) == JNI_TRUE)
        return; /* Exception Thrown */

    return (jint)wl_client_add_resource(client, resource);
}

JNIEXPORT jobject JNICALL
Java_org_freedesktop_wayland_server_Client_getDisplay(JNIEnv * env,
        jobject jclient)
{
    struct wl_client * client = wl_jni_client_from_java(env, jclient);
    return wl_jni_display_to_java(env, wl_client_get_display(client));
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Client_destroy(JNIEnv *env,
        jobject jclient)
{
    struct wl_client *client;
    
    client = wl_jni_client_from_java(env, jclient);

    if (client)
        wl_client_destroy(client);
}

JNIEXPORT void JNICALL
Java_org_freedesktop_wayland_server_Client_initializeJNI(JNIEnv * env,
        jclass cls)
{
    Client.class = (*env)->NewGlobalRef(env, cls);
    if (Client.class == NULL)
        return; /* Exception Thrown */

    Client.init_long = (*env)->GetMethodID(env, Client.class,
            "<init>", "(J)V");
    if (Client.init_long == NULL)
        return; /* Exception Thrown */

    Client.init_display_int = (*env)->GetMethodID(env, Client.class,
            "<init>", "(Lorg/freedesktop/wayland/server/Display;I)V");
    if (Client.init_display_int == NULL)
        return; /* Exception Thrown */
}

static void
ensure_client_object_cache(JNIEnv * env, jclass cls)
{
    if (Client.class != NULL);
        return;

    if (cls == NULL) {
        cls = (*env)->FindClass(env, "org/freedesktop/wayland/server/Client");
        if (cls == NULL)
            return;
        Java_org_freedesktop_wayland_server_Client_initializeJNI(env, cls);
        (*env)->DeleteLocalRef(env, cls);
    } else {
        Java_org_freedesktop_wayland_server_Client_initializeJNI(env, cls);
    }
}

