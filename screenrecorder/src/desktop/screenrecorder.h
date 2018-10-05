#ifndef screenrecorder_h
#define screenrecorder_h

#include <stdint.h>
#include <stddef.h>
#if defined(DM_PLATFORM_OSX)
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
#elif defined(DM_PLATFORM_LINUX)
	#include <GL/gl.h>
	#include <GL/glext.h>

	#define GET_PROC_ADDRESS(function, name, type) function = (type)glXGetProcAddress((const GLubyte *)name);\
	if (function == 0x0) function = (type)glXGetProcAddress((const GLubyte *)name "ARB");\
	if (function == 0x0) function = (type)glXGetProcAddress((const GLubyte *)name "EXT");\
	if (function == 0x0) dmLogError("Could not find gl function %s.", name);
#elif defined(DM_PLATFORM_WINDOWS)
	#include <Windows.h>
	#include <gl/GL.h>
	#include <gl/glext.h>
	#include <wingdi.h>

	#define GET_PROC_ADDRESS(function, name, type) function = (type)wglGetProcAddress(name);\
	if (function == 0x0) function = (type)wglGetProcAddress(name "ARB");\
	if (function == 0x0) function = (type)wglGetProcAddress(name "EXT");\
	if (function == 0x0) dmLogError("Could not find gl function %s.", name);
#elif defined(DM_PLATFORM_HTML5)
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
	#include <emscripten.h>
#endif

#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
#include <thread.h>

#include "sdk.h"
#include "circular_buffer.h"
#include "webmwriter.h"

struct CaptureParams {
	char *filename;
	int *width;
	int *height;
	int *bitrate;
	int *iframe;
	int *fps;
	double *duration;
	double *x_scale;
	double *y_scale;
	int texture_id;
	bool *async_encoding;
};

class ScreenRecorder {
private:
	#ifdef DM_PLATFORM_HTML5
		uint8_t *pixels;
	#endif
	GLuint scaled_texture;
	GLuint shader_program;
	GLuint vertex_buffer;
	GLint position_attrib;
	GLint texcoord_attrib;
	GLint tex_uniform;
	GLint scale_uniform;
	GLint resolution_uniform;
	GLuint fbo;
	GLuint pbo[3];
	int pbo_index;
	bool is_pbo_full;
	vpx_image_t image;
	vpx_codec_enc_cfg_t encoder_config;
	vpx_codec_ctx_t codec;
	int frame_count;
	CircularBuffer *circular_buffer;
	WebmWriter webm_writer;
	thread_ptr_t encoding_thread;
	bool is_initialized;
public:
	bool should_encoding_thread_exit;
	thread_signal_t encoding_signal;
	thread_signal_t encoding_done_signal;
	bool is_enconding_thread_available;
	CaptureParams capture_params;
	ScreenRecorder();
	~ScreenRecorder();
	bool init(char *error_message);
	bool start(char *error_message);
	bool stop(char *error_message);
	bool capture_frame(char *error_message);
	bool encode_frame(bool is_flush);
};

#endif
