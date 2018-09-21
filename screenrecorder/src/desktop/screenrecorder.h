#ifndef screenrecorder_h
#define screenrecorder_h

#include <stdint.h>
#include <stddef.h>
#ifdef __APPLE__
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
#else
	#ifdef _WIN32
		#include <Windows.h>
		#include <gl/GL.h>
	#else
		#include <GL/gl2.h>
		#include <GL/gl2ext.h>
	#endif
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
	bool init();
	bool start();
	void stop();
	void capture_frame();
	bool encode_frame(bool is_flush);
};

#endif
