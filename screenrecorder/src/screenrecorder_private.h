#ifndef screenrecorder_private_h
#define screenrecorder_private_h

// The name of the extension affects Lua module name and Java package name.
#define EXTENSION_NAME screenrecorder

// Convert extension name to C const string.
#define STRINGIFY(s) #s
#define STRINGIFY_EXPANDED(s) STRINGIFY(s)
#define EXTENSION_NAME_STRING STRINGIFY_EXPANDED(EXTENSION_NAME)

#include <dmsdk/sdk.h>

// The following functions are implemented for each platform.
// Lua API.
int ScreenRecorder_init(lua_State *L);
int ScreenRecorder_start(lua_State *L);
int ScreenRecorder_stop(lua_State *L);
int ScreenRecorder_mux_audio_video(lua_State *L);
int ScreenRecorder_capture_frame(lua_State *L);
int ScreenRecorder_is_recording(lua_State *L);
int ScreenRecorder_is_preview_available(lua_State *L);
int ScreenRecorder_show_preview(lua_State *L);
// Extension lifecycle functions.
void ScreenRecorder_initialize(lua_State *L);
void ScreenRecorder_update(lua_State *L);
void ScreenRecorder_finalize(lua_State *L);

#if defined(DM_PLATFORM_ANDROID)

namespace {
	// JNI access is only valid on an attached thread.
	struct ThreadAttacher {
		JNIEnv *env;
		bool has_attached;
		ThreadAttacher() : env(NULL), has_attached(false) {
			if (dmGraphics::GetNativeAndroidJavaVM()->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
				dmGraphics::GetNativeAndroidJavaVM()->AttachCurrentThread(&env, NULL);
				has_attached = true;
			}
		}
		~ThreadAttacher() {
			if (has_attached) {
				if (env->ExceptionCheck()) {
					env->ExceptionDescribe();
				}
				env->ExceptionClear();
				dmGraphics::GetNativeAndroidJavaVM()->DetachCurrentThread();
			}
		}
	};

	// Dynamic Java class loading.
	struct ClassLoader {
		private:
			JNIEnv *env;
			jobject class_loader_object;
			jmethodID load_class;
		public:
			ClassLoader(JNIEnv *env) : env(env) {
				jclass activity_class = env->FindClass("android/app/NativeActivity");
				jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
				jmethodID get_class_loader = env->GetMethodID(activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
				load_class = env->GetMethodID(class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
				class_loader_object = env->CallObjectMethod(dmGraphics::GetNativeAndroidActivity(), get_class_loader);
			}
			jclass load(const char *class_name) {
				jstring class_name_string = env->NewStringUTF(class_name);
				jclass loaded_class = (jclass)env->CallObjectMethod(class_loader_object, load_class, class_name_string);
				env->DeleteLocalRef(class_name_string);
				return loaded_class;
			}
	};
}

#endif
#endif
