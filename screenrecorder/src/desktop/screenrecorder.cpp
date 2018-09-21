#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX)// || defined(DM_PLATFORM_WINDOWS)

#include <string>

#include "screenrecorder.h"
#include "utils.h"

static const char *vertex_shader_source = "#version 110\n"
	"attribute vec2 position;"
	"attribute vec2 texcoord;"
	"varying vec2 var_texcoord;"
	"void main() {"
		"var_texcoord = texcoord;"
		"gl_Position = vec4(position.x, position.y, 0.0, 1.0);"
	"}";


static const char *fragment_shader_source = "#version 110\n"
	"varying vec2 var_texcoord;"
	"uniform sampler2D tex0;"
	"uniform vec2 scale;"
	"uniform vec2 resolution;\n"

	"#define Y_COMPONENT 0\n"
	"#define U_COMPONENT 1\n"
	"#define V_COMPONENT 2\n"

	"float width = resolution.x;"
	"float height = resolution.y;"

	// For scaling. Return black color if source pixel is outside of the image area.
	"vec4 get_pixel(vec2 source) {"
		"source = (source - 0.5) / scale + 0.5;"
		"if (source.x >= 0.0 && source.y >= 0.0 && source.x <= 1.0 && source.y <= 1.0) return texture2D(tex0, source);"
		"return vec4(0.0, 0.0, 0.0, 1.0);"
	"}"

	"float rgba_to_yuv(int component, float offset) {"
		"if (component == Y_COMPONENT) {"
			"vec4 rgba = get_pixel(vec2(mod(offset, width) / width, (1.0 - floor(offset / width) / height)));"			
			"return 0.299 * rgba.r + 0.587 * rgba.g + 0.114 * rgba.b;"
		"} else if (component == U_COMPONENT || component == V_COMPONENT) {"
			"vec4 rgba = get_pixel(vec2((mod(offset, width) - 1.0) / width, (1.0 - (floor(offset / width) - 3.0) / height * 2.0)));"
			"if (component == U_COMPONENT) {"
				"return -0.169 * rgba.r - 0.331 * rgba.g + 0.5 * rgba.b + 0.5;"
			"} else {"
				"return 0.5 * rgba.r - 0.419 * rgba.g - 0.081 * rgba.b + 0.5;"
			"}"
		"}"
		"return 0.0;"
	"}"

	"void main() {"
		"if (var_texcoord.y < 0.3333333333) {"
			// Y component.
			"float offset = 3.0 * width * ((var_texcoord.x - 0.5) + var_texcoord.y * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(Y_COMPONENT, offset),"
				"rgba_to_yuv(Y_COMPONENT, ++offset),"
				"rgba_to_yuv(Y_COMPONENT, ++offset),"
				"1.0"
			");"
		"} else if (var_texcoord.y < 0.4166666666) {"
			// U component.
			"float offset = 6.0 * width * (var_texcoord.x + (var_texcoord.y - 0.3333333333) * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(U_COMPONENT, offset),"
				"rgba_to_yuv(U_COMPONENT, offset += 2.0),"
				"rgba_to_yuv(U_COMPONENT, offset += 2.0),"
				"1.0"
			");"
		"} else if (var_texcoord.y < 0.5) {"
			// V component.
			"float offset = 6.0 * width * (var_texcoord.x + (var_texcoord.y - 0.4166666666) * height);"
			"gl_FragColor = vec4("
				"rgba_to_yuv(V_COMPONENT, offset),"
				"rgba_to_yuv(V_COMPONENT, offset += 2.0),"
				"rgba_to_yuv(V_COMPONENT, offset += 2.0),"
				"1.0"
			");"
		"} else {"
			// Unused.
			"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);"
		"}"
	"}";

// Quad model.
static float quad_model[] = {
	// position		// texture coords
	-1.0f, -1.0f,	0.0f, 0.0f, // left bottom
	 1.0f, -1.0f,	1.0f, 0.0f, // right bottom
	-1.0f,  1.0f,	0.0f, 1.0f, // left top
	-1.0f,  1.0f,	0.0f, 1.0f, // left top
	 1.0f, -1.0f,	1.0f, 0.0f, // right bottom
	 1.0f,  1.0f,	1.0f, 1.0f, // right top
};

static const int PBO_COUNT = 3;

static int encoding_thread_proc(void *user_data) {
	thread_set_high_priority();
	ScreenRecorder *sr = static_cast<ScreenRecorder *>(user_data);
	while (true) {
		thread_signal_wait(&sr->encoding_signal, THREAD_SIGNAL_WAIT_INFINITE);
		if (!sr->should_encoding_thread_exit) {
			sr->is_enconding_thread_available = false;
			sr->encode_frame(false);
			thread_signal_raise(&sr->encoding_done_signal);
			sr->is_enconding_thread_available = true;
		} else {
			break;
		}
	}
	return 0;
}

ScreenRecorder::ScreenRecorder() :
	scaled_texture(0),
	shader_program(0),
	vertex_buffer(0),
	position_attrib(0),
	texcoord_attrib(0),
	tex_uniform(0),
	scale_uniform(0),
	resolution_uniform(0),
	fbo(0),
	pbo_index(0),
	is_pbo_full(false),
	frame_count(0),
	circular_buffer(NULL),
	encoding_thread(NULL),
	is_initialized(false),
	should_encoding_thread_exit(false),
	is_enconding_thread_available(true),
	capture_params() {
	}

ScreenRecorder::~ScreenRecorder() {
	is_initialized = false;
	if (glIsProgram(shader_program)) {
		glDeleteProgram(shader_program);
		shader_program = 0;
		GLenum error = glGetError(); if (error) dmLogError("glDeleteProgram: %d", error);
	}
	if (glIsBuffer(vertex_buffer)) {
		glDeleteBuffers(1, &vertex_buffer);
		vertex_buffer = 0;
		GLenum error = glGetError(); if (error) dmLogError("glDeleteBuffers: %d", error);
	}
	if (encoding_thread != NULL) {
		dmLogDebug("Finishing encoding thread.");
		// Finish encoding thread.
		should_encoding_thread_exit = true;
		thread_signal_raise(&encoding_signal);
		thread_join(encoding_thread);
		thread_destroy(encoding_thread);
		thread_signal_term(&encoding_signal);
		dmLogDebug("Finished encoding thread.");
	}
}

bool ScreenRecorder::init() {
	if (is_initialized) {
		return true;
	}
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	GLenum error = glGetError(); if (error) {dmLogError("glCreateShader: %d", error); return false;}
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	error = glGetError(); if (error) {dmLogError("glShaderSource: %d", error); return false;}
	glCompileShader(vertex_shader);

	GLint status;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		dmLogError("Failed to compile vertex shader");
		char buffer[512];
		glGetShaderInfoLog(vertex_shader, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		dmLogError("Failed to compile fragment shader");
		char buffer[512];
		glGetShaderInfoLog(fragment_shader, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	shader_program = glCreateProgram();
	error = glGetError(); if (error) {dmLogError("glCreateProgram: %d", error); return false;}
	glAttachShader(shader_program, vertex_shader);
	error = glGetError(); if (error) {dmLogError("glAttachShader v: %d", error); return false;}
	glAttachShader(shader_program, fragment_shader);
	error = glGetError(); if (error) {dmLogError("glAttachShader f: %d", error); return false;}
	glLinkProgram(shader_program);
	error = glGetError(); if (error) {dmLogError("glLinkProgram: %d", error); return false;}
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	if (!status) {
		dmLogError("Failed to link shader");
		char buffer[512];
		glGetProgramInfoLog(shader_program, 512, NULL, buffer);
		dmLogError(buffer);
		return false;
	}

	glDeleteShader(vertex_shader);
	error = glGetError(); if (error) {dmLogError("glDeleteShader v: %d", error); return false;}
	glDeleteShader(fragment_shader);
	error = glGetError(); if (error) {dmLogError("glDeleteShader f: %d", error); return false;}

	glGenBuffers(1, &vertex_buffer);
	error = glGetError(); if (error) {dmLogError("glGenBuffers: %d", error); return false;}
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	error = glGetError(); if (error) {dmLogError("glBindBuffer: %d", error); return false;}
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_model), quad_model, GL_STATIC_DRAW);
	error = glGetError(); if (error) {dmLogError("glBufferData: %d", error); return false;}

	tex_uniform = glGetUniformLocation(shader_program, "tex0");
	error = glGetError(); if (error) {dmLogError("glGetUniformLocation position: %d", error); return false;}

	scale_uniform = glGetUniformLocation(shader_program, "scale");
	error = glGetError(); if (error) dmLogError("glGetUniformLocation scale: %d", error);

	resolution_uniform = glGetUniformLocation(shader_program, "resolution");
	error = glGetError(); if (error) dmLogError("glGetUniformLocation resolution: %d", error);

	// position attribute.
	position_attrib = glGetAttribLocation(shader_program, "position");
	error = glGetError(); if (error) {dmLogError("glGetAttribLocation position: %d", error); return false;}
	glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	error = glGetError(); if (error) {dmLogError("glVertexAttribPointer position: %d", error); return false;}

	// texcoord attribute.
	texcoord_attrib = glGetAttribLocation(shader_program, "texcoord");
	error = glGetError(); if (error) {dmLogError("glGetAttribLocation texcoord: %d", error); return false;}
	glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	error = glGetError(); if (error) {dmLogError("glVertexAttribPointer texcoord: %d", error); return false;}

	is_initialized = true;

	thread_signal_init(&encoding_signal);
	thread_signal_init(&encoding_done_signal);

	return true;
}

bool ScreenRecorder::start() {
	frame_count = 0;
	int width = *capture_params.width;
	int height = *capture_params.height;

	// Cleaning up here, because in the stop_thread it would crash.
	if (glIsFramebuffer(fbo)) {
		glDeleteFramebuffers(1, &fbo);
		glDeleteBuffers(PBO_COUNT, pbo);
		glDeleteTextures(1, &scaled_texture);
	}

	glGenFramebuffers(1, &fbo);
	GLenum error = glGetError(); if (error) dmLogError("glGenFramebuffers fbo: %d", error);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	error = glGetError(); if (error) dmLogError("glBindFramebuffer fbo: %d", error);
	glGenTextures(1, &scaled_texture);
	error = glGetError(); if (error) dmLogError("glGenTextures scaled_texture: %d", error);
	glBindTexture(GL_TEXTURE_2D, scaled_texture);
	error = glGetError(); if (error) dmLogError("glBindTexture scaled_texture: %d", error);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	error = glGetError(); if (error) dmLogError("glTexImage2D scaled_texture: %d", error);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	error = glGetError(); if (error) dmLogError("glTexParameteri scaled_texture: %d", error);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	error = glGetError(); if (error) dmLogError("glTexParameteri scaled_texture: %d", error);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, scaled_texture, 0);
	error = glGetError(); if (error) dmLogError("glFramebufferTexture fbo scaled_texture: %d", error);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) dmLogError("glCheckFramebufferStatus: %d", error);
	GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, draw_buffers);
	error = glGetError(); if (error) dmLogError("glDrawBuffers: %d", error);
	glBindTexture(GL_TEXTURE_2D, 0);
	error = glGetError(); if (error) dmLogError("glBindTexture 0: %d", error);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	error = glGetError(); if (error) dmLogError("glBindFramebuffer 0: %d", error);

	pbo_index = 0;
	is_pbo_full = false;
	glGenBuffers(PBO_COUNT, pbo);
	error = glGetError(); if (error) dmLogError("glGenBuffers pbo: %d", error);
	for (int i = 0; i < PBO_COUNT; ++i) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[i]);
		error = glGetError(); if (error) dmLogError("glBindBuffer pbo[%d]: %d", i, error);
		glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 3, NULL, GL_STREAM_READ);
		error = glGetError(); if (error) dmLogError("glBufferData %d: %d", i, error);
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	error = glGetError(); if (error) dmLogError("glBindBuffer 0: %d", error);

	if (!vpx_img_alloc(&image, VPX_IMG_FMT_I420, width, height, 1)) {
		dmLogError("Failed to allocate image.");
		return false;
	}

	vpx_codec_iface_t *codec_interface = vpx_codec_vp8_cx();
	if (vpx_codec_enc_config_default(codec_interface, &encoder_config, 0)) {
		dmLogError("Failed to get default codec config.");
		return false;
	}

	encoder_config.g_w = width;
	encoder_config.g_h = height;
	encoder_config.g_timebase.num = 1;
	encoder_config.g_timebase.den = *capture_params.fps;
	encoder_config.rc_target_bitrate = *capture_params.bitrate;
	encoder_config.g_threads = 4;
	encoder_config.g_pass = VPX_RC_ONE_PASS;
	encoder_config.rc_end_usage = VPX_VBR;
	encoder_config.rc_resize_allowed = 0;
	encoder_config.rc_min_quantizer = 10;
	encoder_config.rc_max_quantizer = 50;
	encoder_config.rc_buf_initial_sz = 4000;
	encoder_config.rc_buf_optimal_sz = 5000;
	encoder_config.rc_buf_sz = 6000;
	encoder_config.rc_dropframe_thresh = 25;
	encoder_config.kf_mode = VPX_KF_AUTO;
	encoder_config.kf_max_dist = *capture_params.iframe * *capture_params.fps;

	if (vpx_codec_enc_init(&codec, codec_interface, &encoder_config, 0)) {
		dmLogError("Failed to initialize encoder");
		return false;
	}

	if (capture_params.duration != NULL) {
		circular_buffer = new CircularBuffer();
		double duration = *capture_params.duration + *capture_params.iframe; // Increase duration by keyframe interval.
		size_t buffer_size = 1.5 * duration * (*capture_params.bitrate / 8); // Allocate enough memory for frames, plus a bit more for bitrate fluctuation.
		if (!circular_buffer->init(buffer_size, duration * *capture_params.fps)) {
			dmLogError("Failed to initialize circular encoder, requested %lld bytes.", buffer_size);
			return false;
		}
	}

	if (!webm_writer.open(capture_params.filename, width, height, *capture_params.fps)) {
		dmLogError("Failed to open %s for writing.", capture_params.filename);
		return false;
	}

	should_encoding_thread_exit = false;
	is_enconding_thread_available = true;
	if (*capture_params.async_encoding) {
		encoding_thread = thread_create(encoding_thread_proc, this, "Encoding thread", THREAD_STACK_SIZE_DEFAULT);
	} else {
		encoding_thread = NULL;
	}

	return true;
}

void ScreenRecorder::capture_frame() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	GLenum error = glGetError(); if (error) dmLogError("glBindFramebuffer fbo: %d", error);

	glViewport(0, 0, *capture_params.width, *capture_params.height);
	error = glGetError(); if (error) dmLogError("glViewport: %d", error);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shader_program);
	error = glGetError(); if (error) dmLogError("glUseProgram: %d", error);

	glActiveTexture(GL_TEXTURE0);
	error = glGetError(); if (error) dmLogError("glActiveTexture: %d", error);
	glBindTexture(GL_TEXTURE_2D, capture_params.texture_id);
	error = glGetError(); if (error) dmLogError("glBindTexture capture_params.texture_id: %d", error);
	glUniform1i(tex_uniform, 0);
	error = glGetError(); if (error) dmLogError("glUniform1i: %d", error);

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	error = glGetError(); if (error) dmLogError("glBindBuffer: %d", error);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_model), quad_model, GL_STATIC_DRAW);
	error = glGetError(); if (error) dmLogError("glBufferData: %d", error);

	glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	error = glGetError(); if (error) dmLogError("glVertexAttribPointer position: %d", error);
	glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	error = glGetError(); if (error) dmLogError("glVertexAttribPointer texcoord: %d", error);

	glEnableVertexAttribArray(position_attrib);
	error = glGetError(); if (error) dmLogError("glEnableVertexAttribArray position_attrib: %d", error);

	glEnableVertexAttribArray(texcoord_attrib);
	error = glGetError(); if (error) dmLogError("glEnableVertexAttribArray texcoord_attrib: %d", error);

	glUniform2f(scale_uniform, *capture_params.x_scale, *capture_params.y_scale);
	error = glGetError(); if (error) dmLogError("glUniform2f scale: %d", error);

	glUniform2f(resolution_uniform, *capture_params.width, *capture_params.height);
	error = glGetError(); if (error) dmLogError("glUniform2f resolution: %d", error);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	error = glGetError(); if (error) dmLogError("glDrawArrays: %d", error);

	glBindTexture(GL_TEXTURE_2D, 0);
	error = glGetError(); if (error) dmLogError("glBindTexture 0: %d", error);

	glDisableVertexAttribArray(position_attrib);
	error = glGetError(); if (error) dmLogError("glDisableVertexAttribArray position_attrib: %d", error);
	glDisableVertexAttribArray(texcoord_attrib);
	error = glGetError(); if (error) dmLogError("glDisableVertexAttribArray texcoord_attrib: %d", error);

	glUseProgram(0);
	error = glGetError(); if (error) dmLogError("glUseProgram 0: %d", error);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	error = glGetError(); if (error) dmLogError("glBindBuffer 0: %d", error);
	
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pbo_index]);
	error = glGetError(); if (error) dmLogError("glBindBuffer GL_PIXEL_PACK_BUFFER: %d", error);

	GLint is_mapped = GL_FALSE;
	glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER, GL_BUFFER_MAPPED, &is_mapped);
	if (is_mapped) {
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		error = glGetError(); if (error) dmLogError("glUnmapBuffer GL_PIXEL_PACK_BUFFER: %d", error);
	}

	glReadPixels(0, 0, *capture_params.width, *capture_params.height / 2, GL_RGB, GL_UNSIGNED_BYTE, 0);
	error = glGetError(); if (error) dmLogError("glReadPixels: %d", error);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	error = glGetError(); if (error) dmLogError("glBindBuffer GL_PIXEL_PACK_BUFFER 0: %d", error);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	error = glGetError(); if (error) dmLogError("glBindFramebuffer 0: %d", error);

	if (is_pbo_full) {
		if (*capture_params.async_encoding && !is_enconding_thread_available) {
			thread_signal_wait(&encoding_done_signal, THREAD_SIGNAL_WAIT_INFINITE);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[(pbo_index + 1) % PBO_COUNT]);
		GLubyte *pixels = (GLubyte *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		error = glGetError(); if (error) dmLogError("glMapBuffer: %d", error);
		int w = *capture_params.width;
		int h = *capture_params.height;
		if (pixels) {
			image.planes[0] = pixels; // Y frame.
			image.planes[1] = image.planes[0] + w * h; // U frame.
			image.planes[2] = image.planes[1] + w * h / 4; // V frame.
			if (*capture_params.async_encoding) {
				// Signal encoding thread to start encoding.
				thread_signal_raise(&encoding_signal);
			} else {
				encode_frame(false);
			}
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	} else if (pbo_index == PBO_COUNT - 1) {
		is_pbo_full = true;
	}

	pbo_index = (pbo_index + 1) % PBO_COUNT;
}

void ScreenRecorder::stop() {
	if (encoding_thread != NULL) {
		dmLogDebug("Finishing encoding thread.");
		// Finish encoding thread.
		should_encoding_thread_exit = true;
		thread_signal_raise(&encoding_signal);
		thread_join(encoding_thread);
		thread_destroy(encoding_thread);
		dmLogDebug("Finished encoding thread.");
		encoding_thread = NULL;
	}
	// Flush encoder.
	while (encode_frame(true)) {
	}
	vpx_img_free(&image);
	if (vpx_codec_destroy(&codec)) {
		dmLogError("Failed to destroy codec.");
	}
	if (circular_buffer != NULL) {
		int64_t first_timestamp = 0;
		bool got_keyframe = false;
		uint8_t *data = NULL;
		size_t size = 0;
		int64_t timestamp = 0;
		bool is_keyframe = false;
		uint32_t frame_index = 0;
		while (circular_buffer->get_frame(&data, &size, &timestamp, &is_keyframe, &frame_index)) {
			if (!got_keyframe && is_keyframe) {
				got_keyframe = true;
				first_timestamp = timestamp; // Timestamps must start from 0.
			}
			// Must wait for a keyframe first.
			if (got_keyframe) {
				if (!webm_writer.write_frame(data, size, timestamp - first_timestamp, is_keyframe)) {
					dmLogError("Failed to write compressed frame %d.", frame_index);
					break;
				}
			}
		}
		delete circular_buffer;
		circular_buffer = NULL;
	}
	webm_writer.close();
}

bool ScreenRecorder::encode_frame(bool is_flush) {
	bool has_packets = false;
	vpx_codec_iter_t iter = NULL;
	const vpx_codec_cx_pkt_t *pkt = NULL;
	const vpx_codec_err_t res = vpx_codec_encode(&codec, is_flush ? NULL : &image, is_flush ? -1 : frame_count++, 1, 0, VPX_DL_REALTIME);
	if (res != VPX_CODEC_OK) {
		dmLogError("Failed to encode frame.");
		return false;
	}
	while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL) {
		has_packets = true;
		if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
			if (circular_buffer != NULL) {
				if (!circular_buffer->add_frame(static_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz, pkt->data.frame.pts, pkt->data.frame.flags & VPX_FRAME_IS_KEY)) {
					dmLogError("Failed to add compressed frame %d to the circular encoder.", frame_count);
				}
			} else if (!webm_writer.write_frame(static_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz, pkt->data.frame.pts, pkt->data.frame.flags & VPX_FRAME_IS_KEY)) {
				dmLogError("Failed to write compressed frame %d.", frame_count);
			}
		}
	}
	return has_packets;
}

#endif