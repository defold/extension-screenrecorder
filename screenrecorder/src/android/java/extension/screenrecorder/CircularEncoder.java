/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package extension.screenrecorder;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.Surface;

import java.io.File;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.Objects;

/**
 * Encodes video in a fixed-size circular buffer.
 * <p>
 * The obvious way to do this would be to store each packet in its own buffer and hook it
 * into a linked list.  The trouble with this approach is that it requires constant
 * allocation, which means we'll be driving the GC to distraction as the frame rate and
 * bit rate increase.  Instead we create fixed-size pools for video data and metadata,
 * which requires a bit more work for us but avoids allocations in the steady state.
 * <p>
 * Video must always start with a sync frame (a/k/a key frame, a/k/a I-frame).  When the
 * circular buffer wraps around, we either need to delete all of the data between the frame at
 * the head of the list and the next sync frame, or have the file save function know that
 * it needs to scan forward for a sync frame before it can start saving data.
 * <p>
 * When we're told to save a snapshot, we create a MediaMuxer, write all the frames out,
 * and then go back to what we were doing.
 */
class CircularEncoder {
	private static final String MIME_TYPE = "video/avc";    // H.264 Advanced Video Coding.
	private EncoderThread encoder_thread;
	private Surface input_surface;
	private MediaCodec encoder;
	private File output_file;

	interface Callback {
		// Called when video file has been saved. 0 - success, fail otherwise.
		void on_video_saved(int status);
	}

	/**
	 * Configures encoder, and prepares the input Surface.
	 *
	 * @param width Width of encoded video, in pixels.  Should be a multiple of 16.
	 * @param height Height of encoded video, in pixels.  Usually a multiple of 16 (1080 is ok).
	 * @param bitrate Target bit rate, in bits.
	 * @param framerate Expected frame rate.
	 * @param iframe Keyframe interval.
	 * @param duration How many seconds of video we want to have in our buffer at any time.
	 */
	CircularEncoder(int width, int height, int bitrate, int framerate, int iframe, int duration, String filename, Callback callback) throws IOException {
		CircularEncoderBuffer encoder_buffer = new CircularEncoderBuffer(bitrate, framerate, duration);

		MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, width, height);
		format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
		format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
		format.setInteger(MediaFormat.KEY_FRAME_RATE, framerate);
		format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iframe);

		output_file = new File(filename);
		encoder = MediaCodec.createEncoderByType(MIME_TYPE);
		encoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
		input_surface = encoder.createInputSurface();
		encoder_thread = new EncoderThread(encoder, encoder_buffer, callback);
	}


	void start() {
		encoder.start();
		encoder_thread.start();
	}

	Surface get_input_surface() {
		return input_surface;
	}

	void shutdown() {
		Handler handler = encoder_thread.get_handler();
		handler.sendMessage(handler.obtainMessage(EncoderThread.EncoderHandler.MSG_SHUTDOWN));
		try {
			encoder_thread.join();
		} catch (InterruptedException ie) {
			Utils.log("Encoder thread join() was interrupted.");
		}
		if (encoder != null) {
			encoder.stop();
			encoder.release();
			encoder = null;
		}
	}

	/**
	 * Notifies the encoder thread that a new frame will shortly be provided to the encoder.
	 * <p>
	 * There may or may not yet be data available from the encoder output.  The encoder
	 * has a fair mount of latency due to processing, and it may want to accumulate a
	 * few additional buffers before producing output.  We just need to drain it regularly
	 * to avoid a situation where the producer gets wedged up because there's no room for
	 * additional frames.
	 * <p>
	 * If the caller sends the frame and then notifies us, it could get wedged up.  If it
	 * notifies us first and then sends the frame, we guarantee that the output buffers
	 * were emptied, and it will be impossible for a single additional frame to block
	 * indefinitely.
	 */
	void drain_encoder() {
		Handler handler = encoder_thread.get_handler();
		if (handler != null) {
			handler.sendMessage(handler.obtainMessage(EncoderThread.EncoderHandler.MSG_DRAIN_ENCODER));
		}
	}

	/**
	 * Initiates saving the currently-buffered frames to the specified output file.  The
	 * data will be written as a .mp4 file.  The call returns immediately.  When the file
	 * save completes, the callback will be notified.
	 * <p>
	 * The file generation is performed on the encoder thread, which means we won't be
	 * draining the output buffers while this runs.  It would be wise to stop submitting
	 * frames during this time.
	 */
	void save_video() {
		Handler handler = encoder_thread.get_handler();
		if (handler != null) {
			handler.sendMessage(handler.obtainMessage(EncoderThread.EncoderHandler.MSG_SAVE_VIDEO, output_file));
		}
	}

	/**
	 * Object that encapsulates the encoder thread.
	 * <p>
	 * We want to sleep until there's work to do.  We don't actually know when a new frame
	 * arrives at the encoder, because the other thread is sending frames directly to the
	 * input surface.  We will see data appear at the decoder output, so we can either use
	 * an infinite timeout on dequeueOutputBuffer() or wait() on an object and require the
	 * calling app wake us.  It's very useful to have all of the buffer management local to
	 * this thread -- avoids synchronization -- so we want to do the file muxing in here.
	 * So, it's best to sleep on an object and do something appropriate when awakened.
	 * <p>
	 * This class does not manage the MediaCodec encoder startup/shutdown.  The encoder
	 * should be fully started before the thread is created, and not shut down until this
	 * thread has been joined.
	 */
	private static class EncoderThread extends Thread {
		private MediaCodec encoder;
		private MediaFormat format;
		private MediaCodec.BufferInfo buffer_info;

		private EncoderHandler handler;
		private CircularEncoderBuffer encoder_buffer;
		private CircularEncoder.Callback callback;

		private volatile boolean is_ready = false;

		EncoderThread(MediaCodec encoder, CircularEncoderBuffer encoder_buffer, CircularEncoder.Callback callback) {
			this.encoder = encoder;
			this.encoder_buffer = encoder_buffer;
			this.callback = callback;
			buffer_info = new MediaCodec.BufferInfo();
			setPriority(Thread.MAX_PRIORITY);
		}

		@Override
		public void run() {
			Looper.prepare();
			handler = new EncoderHandler(this);    // must create on encoder thread
			is_ready = true;

			Looper.loop();

			is_ready = false;
			handler = null;
		}


		// Returns the Handler used to send messages to the encoder thread.
		EncoderHandler get_handler() {
			if (is_ready) {
				return handler;
			} else {
				return null;
			}
		}

		// Drains all pending output from the decoder, and adds it to the circular buffer.
		void drain_encoder() {
			final int timeout_usec = 0;     // no timeout -- check for buffers, bail if none
			while (true) {
				int status = encoder.dequeueOutputBuffer(buffer_info, timeout_usec);
				if (status == MediaCodec.INFO_TRY_AGAIN_LATER) {
					// no output available yet
					break;
				} else if (status == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
					// Should happen before receiving buffers, and should only happen once.
					// The MediaFormat contains the csd-0 and csd-1 keys, which we'll need
					// for MediaMuxer.  It's unclear what else MediaMuxer might want, so
					// rather than extract the codec-specific data and reconstruct a new
					// MediaFormat later, we just grab it here and keep it around.
					format = encoder.getOutputFormat();
				} else {
					ByteBuffer encoded_data = encoder.getOutputBuffer(status);
					if (encoded_data == null) {
						Utils.log("CircularEncoder: encoder output buffer " + status + " was null.");
						break;
					}
					if ((buffer_info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
						// The codec config data was pulled out when we got the
						// INFO_OUTPUT_FORMAT_CHANGED status. The MediaMuxer won't accept
						// a single big blob -- it wants separate csd-0/csd-1 chunks --
						// so simply saving this off won't work.
						buffer_info.size = 0;
					}
					if (buffer_info.size != 0) {
						// adjust the ByteBuffer values to match BufferInfo (not needed?)
						encoded_data.position(buffer_info.offset);
						encoded_data.limit(buffer_info.offset + buffer_info.size);
						encoder_buffer.add(encoded_data, buffer_info.flags, buffer_info.presentationTimeUs);
					}
					encoder.releaseOutputBuffer(status, false);
					if ((buffer_info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
						Utils.log("CircularEncoder: reached end of stream unexpectedly.");
						break;
					}
				}
			}
		}

		/**
		 * Saves the encoder output to a .mp4 file.
		 * <p>
		 * We'll drain the encoder to get any lingering data, but we're not going to shut
		 * the encoder down or use other tricks to try to "flush" the encoder.  This may
		 * mean we miss the last couple of submitted frames if they're still working their
		 * way through.
		 * <p>
		 * We may want to reset the buffer after this -- if they hit "capture" again right
		 * away they'll end up saving video with a gap where we paused to write the file.
		 */
		void save_video(File output_file) {
			int index = encoder_buffer.get_first_index();
			if (index < 0) {
				Utils.log("CircularEncoder: Unable to get first index.");
				callback.on_video_saved(1);
				return;
			}
			MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
			MediaMuxer muxer = null;
			int result;
			try {
				muxer = new MediaMuxer(output_file.getPath(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
				int videoTrack = muxer.addTrack(format);
				muxer.start();
				do {
					ByteBuffer buf = encoder_buffer.get_chunk(index, info);
					muxer.writeSampleData(videoTrack, buf, info);
					index = encoder_buffer.get_next_index(index);
				} while (index >= 0);
				result = 0;
			} catch (IOException ioe) {
				Utils.log("CircularEncoder: Muxer failed: " + ioe.toString());
				result = 2;
			} finally {
				if (muxer != null) {
					muxer.stop();
					muxer.release();
				}
			}
			callback.on_video_saved(result);
		}

		private static class EncoderHandler extends Handler {
			private static final int MSG_DRAIN_ENCODER = 1;
			private static final int MSG_SAVE_VIDEO = 2;
			private static final int MSG_SHUTDOWN = 3;

			// This shouldn't need to be a weak ref, since we'll go away when the Looper quits, but no real harm in it.
			private WeakReference<EncoderThread> weak_encoder_thread;

			EncoderHandler(EncoderThread encoder_thread) {
				weak_encoder_thread = new WeakReference<EncoderThread>(encoder_thread);
			}

			@Override
			public void handleMessage(Message msg) {
				EncoderThread encoder_thread = weak_encoder_thread.get();
				if (encoder_thread == null) {
					Utils.log("EncoderHandler.handleMessage: weak ref is null");
					return;
				}

				switch (msg.what) {
					case MSG_DRAIN_ENCODER:
						encoder_thread.drain_encoder();
						break;
					case MSG_SAVE_VIDEO:
						encoder_thread.save_video((File)msg.obj);
						break;
					case MSG_SHUTDOWN:
						Objects.requireNonNull(Looper.myLooper()).quit();
						break;
					default:
						throw new RuntimeException("unknown message " + msg.what);
				}
			}
		}
	}
}
