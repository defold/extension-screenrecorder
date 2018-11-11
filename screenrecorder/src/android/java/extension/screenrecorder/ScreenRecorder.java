package extension.screenrecorder;

import java.io.IOException;
import java.util.Hashtable;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.media.MediaRecorder;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.view.Surface;

import extension.screenrecorder.Utils.Scheme;
import extension.screenrecorder.Utils.Table;

@SuppressWarnings("unused")
public class ScreenRecorder implements MuxerTask.Callback {
	private Activity activity;
	private MediaRecorder media_recorder;
	private CircularEncoder circular_encoder;
	private String circular_encoder_exception;
	private boolean is_circular_encoder_initializing = false;
	private boolean is_initialized = false;
	private boolean is_recording_boolean = false;
	private boolean use_media_recorder = false;
	private int lua_listener = Lua.REFNIL;
	private boolean is_write_external_storage_permission_granted = false;
	private int width, height;
	private Utils.LuaLightuserdata render_target;
	private double x_scale, y_scale;
	private boolean skip_frame = false;
	private static final String SCREENRECORDER = "screenrecorder";
	private static final String EVENT_PHASE = "phase";
	private static final String EVENT_INIT = "init";
	private static final String EVENT_MUXED = "muxed";
	private static final String EVENT_RECORDED = "recorded";
	private static final String EVENT_IS_ERROR = "is_error";
	private static final String EVENT_ERROR_MESSAGE = "error_message";

	@SuppressWarnings({"JniMissingFunction", "BooleanMethodIsAlwaysInverted"})
	private native boolean init_native(int width, int height, long render_target, double x_scale, double y_scale);
	@SuppressWarnings("JniMissingFunction")
	private native void start_native();
	@SuppressWarnings("JniMissingFunction")
	private native void stop_native();

	@SuppressWarnings("unused")
	public ScreenRecorder(android.app.Activity main_activity) {
		activity = main_activity;
		Utils.setTag(SCREENRECORDER);
	}

	// Called from extension_android.cpp.
	@SuppressWarnings("unused")
	public void extension_finalize(long L) {
		if (media_recorder != null) {
			media_recorder.release();
			media_recorder = null;
		}
	}

	// Called from extension_android.cpp each frame.
	@SuppressWarnings("unused")
	public void update(long L) {
		if (!use_media_recorder) {
			if (is_recording_boolean) {
				if (skip_frame) {
					skip_frame = false;
				} else {
					circular_encoder.drain_encoder();
					skip_frame = true;
				}
			} else if (is_circular_encoder_initializing) {
				circular_encoder_init_complete();
			}
		}
		Utils.executeTasks(L);
	}

	@SuppressWarnings("unused")
	public Surface get_encoder_surface() {
		Surface rs;
		if (use_media_recorder) {
			rs = media_recorder.getSurface();
		} else {
			rs = circular_encoder.get_input_surface();
		}
		return rs;
	}

	@SuppressWarnings("BooleanMethodIsAlwaysInverted")
	private boolean check_is_initialized() {
		if (is_initialized) {
			return true;
		} else {
			Utils.log("The extension is not initialized.");
			return false;
		}
	}

	//region Lua functions

	// screenrecorder.init(params)
	@SuppressLint("ObsoleteSdkInt")
	private int init(long L) {
		Utils.checkArgCount(L, 1);
		Scheme scheme = new Scheme()
				.string("filename")
				.number("width")
				.number("height")
				.lightuserdata("render_target")
				.number("x_scale")
				.number("y_scale")
				.number("fps")
				.number("iframe")
				.number("bitrate")
				.number("duration")
				.function("listener");

		Table params = new Table(L, 1).parse(scheme);
		final String filename = params.getStringNotNull("filename");
		width = params.getInteger("width", 1280);
		height = params.getInteger("height", 720);
		render_target = params.getLightuserdataNotNull("render_target");
		x_scale = params.getDouble("x_scale", 1);
		y_scale = params.getDouble("y_scale", 1);
		final int fps = params.getInteger("fps", 30);
		final int iframe = params.getInteger("iframe", 1);
		final int bitrate = params.getInteger("bitrate", 2 * 1024 * 1024);
		final Integer duration = params.getInteger("duration");
		lua_listener = params.getFunction("listener", Lua.REFNIL);

		if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
			Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
			event.put(EVENT_PHASE, EVENT_INIT);
			event.put(EVENT_IS_ERROR, true);
			event.put(EVENT_ERROR_MESSAGE, "Screenrecorder requires Android API 21 or higher.");
			Utils.dispatchEvent(lua_listener, event, true);
			return 0;
		}

		use_media_recorder = duration == null;

		if (use_media_recorder) {
			media_recorder = new MediaRecorder();
			media_recorder.setVideoSource(MediaRecorder.VideoSource.SURFACE);
			media_recorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
			media_recorder.setOutputFile(filename);
			media_recorder.setVideoEncoder(MediaRecorder.VideoEncoder.H264);
			media_recorder.setVideoEncodingBitRate(bitrate);
			media_recorder.setVideoFrameRate(fps);
			media_recorder.setVideoSize(width, height);
			// TODO: media_recorder.setOnErrorListener();
			String error_message = "";
			boolean is_error = false;
			try {
				media_recorder.prepare();
			} catch (IOException e) {
				is_error = true;
				error_message = "Failed to prepare media recorder: " + e.toString();
			}
			if (!is_error) {
				is_error = !init_native(width, height, render_target.pointer, x_scale, y_scale);
			}
			is_initialized = !is_error;
			Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
			event.put(EVENT_PHASE, EVENT_INIT);
			event.put(EVENT_IS_ERROR, is_error);
			if (is_error) {
				event.put(EVENT_ERROR_MESSAGE, error_message);
			}
			Utils.dispatchEvent(lua_listener, event, is_error);
		} else {
			if (duration < iframe * 2) {
				Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
				event.put(EVENT_PHASE, EVENT_INIT);
				event.put(EVENT_IS_ERROR, true);
				event.put(EVENT_ERROR_MESSAGE, "CircularEncoder: requested time span is too short: " + duration + " vs. " + (iframe * 2) + ".");
				Utils.dispatchEvent(lua_listener, event, true);
			} else {
				final ScreenRecorder lua_loader = this;
				activity.runOnUiThread(new Runnable() {
					@Override
					public void run() {
						is_circular_encoder_initializing = true;
						try {
							circular_encoder = new CircularEncoder(width, height, bitrate, fps, iframe, duration, filename, new EncoderHandler(lua_loader));
						} catch (IOException e) {
							circular_encoder_exception = e.toString();
						}
					}
				});
			}
		}

		return 0;
	}

	private void circular_encoder_init_complete() {
		if (circular_encoder == null && circular_encoder_exception == null) {
			return;
		}
		is_circular_encoder_initializing = false;
		String error_message = "";
		boolean is_error = false;
		if (circular_encoder_exception != null) {
			is_error = true;
			error_message = "Failed to initialize circular encoder: " + circular_encoder_exception;
		}
		if (!is_error) {
			is_error = !init_native(width, height, render_target.pointer, x_scale, y_scale);
		}
		is_initialized = !is_error;
		Hashtable<Object, Object> event = Utils.newEvent(SCREENRECORDER);
		event.put(EVENT_PHASE, EVENT_INIT);
		event.put(EVENT_IS_ERROR, is_error);
		if (is_error) {
			event.put(EVENT_ERROR_MESSAGE, error_message);
		}
		Utils.dispatchEvent(lua_listener, event, is_error);
	}

	// screenrecorder.start()
	private int start(long L) {
		Utils.checkArgCount(L, 0);
		if (!check_is_initialized()) return 0;

		is_recording_boolean = true;
		if (use_media_recorder) {
			media_recorder.start();
		} else {
			circular_encoder.start();
		}
		start_native();

		return 0;
	}

	// screenrecorder.stop()
	private int stop(long L) {
		Utils.checkArgCount(L, 0);

		if (!check_is_initialized()) return 0;

		is_recording_boolean = false;
		if (use_media_recorder) {
			media_recorder.stop();
			media_recorder.release();
			media_recorder = null;
			Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
			event.put(EVENT_PHASE, EVENT_RECORDED);
			event.put(EVENT_IS_ERROR, false);
			Utils.dispatchEvent(lua_listener, event);
		} else {
			circular_encoder.save_video();
		}
		is_initialized = false;
		is_circular_encoder_initializing = false;
		circular_encoder_exception = null;
		stop_native();

		return 0;
	}

	// screenrecorder.mux_audio_video(params)
	private int mux_audio_video(long L) {
		Utils.checkArgCount(L, 1);

		Scheme scheme = new Scheme()
				.string("audio_filename")
				.string("video_filename")
				.string("filename");

		Table params = new Table(L, 1).parse(scheme);
		final String audio_filename = params.getStringNotNull("audio_filename");
		final String video_filename = params.getStringNotNull("video_filename");
		final String filename = params.getStringNotNull("filename");

		final ScreenRecorder lua_loader = this;
		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				new MuxerTask(lua_loader).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, audio_filename, video_filename, filename);
			}
		});
		return 0;
	}
	//endregion

	private void on_video_saved(boolean is_error) {
		circular_encoder.shutdown();
		circular_encoder = null;
		Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
		event.put(EVENT_PHASE, EVENT_RECORDED);
		event.put(EVENT_IS_ERROR, is_error);
		if (is_error) {
			event.put(EVENT_ERROR_MESSAGE, "Failed to save the recording.");
		}
		Utils.dispatchEvent(lua_listener, event);
	}

	public void on_audio_video_muxed(String error_message) {
		Hashtable<Object,Object> event = Utils.newEvent(SCREENRECORDER);
		event.put(EVENT_PHASE, EVENT_MUXED);
		event.put(EVENT_IS_ERROR, error_message != null);
		if (error_message != null) {
			event.put(EVENT_ERROR_MESSAGE, error_message);
		}
		Utils.dispatchEvent(lua_listener, event);
	}

	//region CircularEncoder.Callback
	// Receives callback messages from the encoder thread.
	private static class EncoderHandler extends Handler implements CircularEncoder.Callback {
		private static final int MSG_VIDEO_SAVED = 2;
		private static final int MSG_BUFFER_STATUS = 3;
		private ScreenRecorder lua_loader;

		EncoderHandler(ScreenRecorder lua_loader) {
			this.lua_loader = lua_loader;
		}

		@Override
		public void on_video_saved(int status) {
			sendMessage(obtainMessage(MSG_VIDEO_SAVED, status, 0, null));
		}

		@Override
		public void handleMessage(Message msg) {
			if (msg.what == MSG_VIDEO_SAVED) {
				lua_loader.on_video_saved(msg.arg1 != 0);
			}
		}
	}
	//endregion
}
