#ifndef extension_h
#define extension_h

// The name of the extension affects C++/ObjC exported symbols, Lua module name and Java package name.
#define EXTENSION_NAME screenrecorder

// Convert extension name to C const string.
#define STRINGIFY(s) #s
#define STRINGIFY_EXPANDED(s) STRINGIFY(s)
#define EXTENSION_NAME_STRING STRINGIFY_EXPANDED(EXTENSION_NAME)

// Each extension must have unique exported symbols. Construct function names based on the extension name.
#define FUNCTION_NAME(extension_name, function_name) Extension_ ## extension_name ## _ ## function_name
#define FUNCTION_NAME_EXPANDED(extension_name, function_name) FUNCTION_NAME(extension_name, function_name)

#define APP_INITIALIZE FUNCTION_NAME_EXPANDED(EXTENSION_NAME, AppInitialize)
#define APP_FINALIZE FUNCTION_NAME_EXPANDED(EXTENSION_NAME, AppFinalize)
#define INITIALIZE FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Initialize)
#define UPDATE FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Update)
#define FINALIZE FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Finalize)

#define DECLARE_DEFOLD_EXTENSION DM_DECLARE_EXTENSION(EXTENSION_NAME, EXTENSION_NAME_STRING, APP_INITIALIZE, APP_FINALIZE, INITIALIZE, UPDATE, 0, FINALIZE)

// For iOS implementation.
#define APP_INITIALIZE_IOS FUNCTION_NAME_EXPANDED(EXTENSION_NAME, AppInitialize_iOS)
#define APP_FINALIZE_IOS FUNCTION_NAME_EXPANDED(EXTENSION_NAME, AppFinalize_iOS)
#define INITIALIZE_IOS FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Initialize_iOS)
#define UPDATE_IOS FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Update_iOS)
#define FINALIZE_IOS FUNCTION_NAME_EXPANDED(EXTENSION_NAME, Finalize_iOS)

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
