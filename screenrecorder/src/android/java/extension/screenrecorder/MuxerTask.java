package extension.screenrecorder;

import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.AsyncTask;

import java.nio.ByteBuffer;

class MuxerTask extends AsyncTask<String, Void, String> {
	interface Callback {
		void on_audio_video_muxed(String error_message);
	}

	private Callback callback;

	MuxerTask(Callback callback) {
		this.callback = callback;
	}

	protected String doInBackground(String... params) {
		String audio_filename = params[0];
		String video_filename = params[1];
		String filename = params[2];
		try {
			MediaExtractor video_extractor = new MediaExtractor();
			video_extractor.setDataSource(video_filename);

			MediaExtractor audio_extractor = new MediaExtractor();
			audio_extractor.setDataSource(audio_filename);

			MediaMuxer muxer = new MediaMuxer(filename, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);

			video_extractor.selectTrack(0);
			MediaFormat video_format = video_extractor.getTrackFormat(0);
			int video_track = muxer.addTrack(video_format);

			audio_extractor.selectTrack(0);
			int audio_track = muxer.addTrack(audio_extractor.getTrackFormat(0));

			boolean is_end = false;
			int offset = 100;
			int sample_size = 1024 * 1024;
			ByteBuffer video_buffer = ByteBuffer.allocate(sample_size);
			ByteBuffer audio_buffer = ByteBuffer.allocate(sample_size);
			MediaCodec.BufferInfo video_buffer_info = new MediaCodec.BufferInfo();
			MediaCodec.BufferInfo audio_buffer_info = new MediaCodec.BufferInfo();
			video_extractor.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
			audio_extractor.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
			muxer.start();

			long video_duration = video_format.getLong(MediaFormat.KEY_DURATION);

			while (!is_end && !isCancelled()) {
				video_buffer_info.offset = offset;
				video_buffer_info.size = video_extractor.readSampleData(video_buffer, offset);
				if (video_buffer_info.size < 0 || audio_buffer_info.size < 0) {
					is_end = true;
					video_buffer_info.size = 0;
				} else {
					video_buffer_info.presentationTimeUs = video_extractor.getSampleTime();
					video_buffer_info.flags = video_extractor.getSampleFlags();
					muxer.writeSampleData(video_track, video_buffer, video_buffer_info);
					video_extractor.advance();
				}
			}

			is_end = false;
			while (!is_end && !isCancelled()) {
				audio_buffer_info.offset = offset;
				audio_buffer_info.size = audio_extractor.readSampleData(audio_buffer, offset);
				if (video_buffer_info.size < 0 || audio_buffer_info.size < 0) {
					is_end = true;
					audio_buffer_info.size = 0;
				} else {
					long sample_time = audio_extractor.getSampleTime();
					if (sample_time < video_duration) {
						audio_buffer_info.presentationTimeUs = sample_time;
						audio_buffer_info.flags = audio_extractor.getSampleFlags();
						muxer.writeSampleData(audio_track, audio_buffer, audio_buffer_info);
						audio_extractor.advance();
					} else {
						is_end = true;
					}
				}
			}
			muxer.stop();
			muxer.release();
		} catch (Exception e) {
			return e.getMessage();
		}
		return null;
	}

	protected void onPostExecute(String error_message) {
		callback.on_audio_video_muxed(error_message);
	}
}
