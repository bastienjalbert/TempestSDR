#include <jni.h>
#include <stdio.h>
#include "TSDRLibraryNDK.h"
#include "include\TSDRLibrary.h"
#include "include\TSDRCodes.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

tsdr_lib_t tsdr_instance;

#define THROW(x) error(env, x, "")

struct java_context {
		jobject obj;
		jclass cls;
		jfieldID fid_pixels;
		jfieldID fid_width;
		jfieldID fid_height;
		jmethodID fixSize;
		jmethodID notifyCallbacks;
	} typedef java_context_t;

static JavaVM *jvm;
static int javaversion;

void error_translate (int exception_code, char * exceptionclass) {
	switch (exception_code) {
		case TSDR_OK:
			return;
		case TSDR_ERR_PLUGIN:
			strcpy(exceptionclass, "martin/tempest/core/exceptions/TSDRLoadPluginException");
			return;
		case TSDR_NOT_IMPLEMENTED:
			strcpy(exceptionclass, "martin/tempest/core/exceptions/TSDRFunctionNotImplemented");
			return;
		case TSDR_WRONG_WIDTHHEIGHT:
			strcpy(exceptionclass, "martin/tempest/core/exceptions/TSDRWrongWidthHeightException");
			return;
		case TSDR_ALREADY_RUNNING:
			strcpy(exceptionclass, "martin/tempest/core/exceptions/TSDRAlreadyRunningException");
			return;
		default:
			strcpy(exceptionclass, "java/lang/Exception");
			return;
		}
}

void error(JNIEnv * env, int exception_code, const char *inmsg, ...)
{
	if (exception_code == TSDR_OK) return;

	char exceptionclass[256];
	error_translate(exception_code, exceptionclass);

	char msg[256];

	va_list argptr;
	va_start(argptr, inmsg);
	vsprintf(msg, inmsg, argptr);
	va_end(argptr);

    jclass cls = (*env)->FindClass(env, exceptionclass);
    /* if cls is NULL, an exception has already been thrown */
    if (cls != NULL) {
        (*env)->ThrowNew(env, cls, msg);
    }
    /* free the local ref */
    (*env)->DeleteLocalRef(env, cls);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_init (JNIEnv * env, jobject obj) {
	(*env)->GetJavaVM(env, &jvm);
	javaversion = (*env)->GetVersion(env);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_nativeLoadPlugin (JNIEnv * env, jobject obj, jstring  path) {
	const char *npath = (*env)->GetStringUTFChars(env, path, 0);
	THROW(tsdr_loadplugin(&tsdr_instance, npath));
	(*env)->ReleaseStringUTFChars(env, path, npath);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_pluginParams (JNIEnv * env, jobject obj, jstring params) {
	const char *nparams = (*env)->GetStringUTFChars(env, params, 0);
	THROW(tsdr_pluginparams(&tsdr_instance, nparams));
	(*env)->ReleaseStringUTFChars(env, params, nparams);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setSampleRate (JNIEnv * env, jobject obj, jlong rate) {
	THROW(tsdr_setsamplerate(&tsdr_instance, (uint32_t) rate));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setBaseFreq (JNIEnv * env, jobject obj, jlong freq) {
	THROW(tsdr_setbasefreq(&tsdr_instance, (uint32_t) freq));
}

void read_async(float *buf, int width, int height, void *ctx) {
	java_context_t * context = (java_context_t *) ctx;
	JNIEnv *env;

	if ((*jvm)->GetEnv(jvm, (void **)&env, javaversion) == JNI_EDETACHED)
		(*jvm)->AttachCurrentThread(jvm, (void **) &env, 0);

	jint i_width = (*env)->GetIntField(env, context->obj, context->fid_width);
	jint i_height = (*env)->GetIntField(env, context->obj, context->fid_height);

	if (i_width != width || i_height != height) {
		// fixSize(200, 200);
		(*env)->CallVoidMethod(env, context->obj, context->fixSize, width, height);
	}

	jintArray pixels_obj = (*env)->GetObjectField(env, context->obj, context->fid_pixels);
	jint * pixels = (*env)->GetIntArrayElements(env,pixels_obj,0);
	jint * data = pixels;

	int i;
	const int size = width * height;
	for (i = 0; i < size; i++) {
		const int col = (int) (*(buf++) * 255.0f);
		*(data++) = col | (col << 8) | (col << 16);
	}

	// release elements
	(*env)->ReleaseIntArrayElements(env,pixels_obj,pixels,0);

	// notifyCallbacks();
	(*env)->CallVoidMethod(env, context->obj, context->notifyCallbacks);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_nativeStart (JNIEnv * env, jobject obj) {

	java_context_t * context = (java_context_t *) malloc(sizeof(java_context_t));

	context->obj = (*env)->NewGlobalRef(env, obj);
	(*env)->DeleteLocalRef(env, obj);
	context->cls = (jclass) (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, context->obj));
	context->fid_pixels = (*env)->GetFieldID(env, context->cls, "pixels", "[I");
	context->fid_width = (*env)->GetFieldID(env, context->cls, "width", "I");
	context->fid_height = (*env)->GetFieldID(env, context->cls, "height", "I");
	context->fixSize = (*env)->GetMethodID(env, context->cls, "fixSize", "(II)V");
	context->notifyCallbacks = (*env)->GetMethodID(env, context->cls, "notifyCallbacks", "()V");

	THROW(tsdr_readasync(&tsdr_instance, read_async, (void *) context));

	(*env)->DeleteGlobalRef(env, context->obj);
	(*env)->DeleteGlobalRef(env, context->cls);
	free(context);
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_stop (JNIEnv * env, jobject obj) {
	THROW(tsdr_stop(&tsdr_instance));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setGain (JNIEnv * env, jobject obj, jfloat gain) {
	THROW(tsdr_setgain(&tsdr_instance, (float) gain));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_unloadPlugin (JNIEnv * env, jobject obj) {
	THROW(tsdr_unloadplugin(&tsdr_instance));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setResolution (JNIEnv * env, jobject obj, jint width, jint height) {
	THROW(tsdr_setresolution(&tsdr_instance, (int) width, (int) height));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setVfreq (JNIEnv * env, jobject obj, jfloat freq) {
	THROW(tsdr_setvfreq(&tsdr_instance, (float) freq));
}

JNIEXPORT void JNICALL Java_martin_tempest_core_TSDRLibrary_setHfreq (JNIEnv * env, jobject obj, jfloat freq) {
	THROW(tsdr_sethfreq(&tsdr_instance, (float) freq));
}
