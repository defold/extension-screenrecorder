#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)

#include "webmwriter.h"
#include "utils.h"

WebmWriter::WebmWriter() :
	file(NULL),
	frame_ns(0),
	writer(NULL),
	segment(NULL) {
}

WebmWriter::~WebmWriter() {
	close();
}

bool WebmWriter::open(const char *filename, int width, int height, int fps) {
	frame_ns = 1000000000ll / fps;
	file = fopen(filename, "wb");
	if (!file) {
		return false;
	}

	writer = new mkvmuxer::MkvWriter(file);
	segment = new mkvmuxer::Segment();
	segment->Init(writer);
	segment->set_mode(mkvmuxer::Segment::kFile);
	segment->OutputCues(true);

	mkvmuxer::SegmentInfo *info = segment->GetSegmentInfo();
	info->set_timecode_scale(1000000);
	info->set_writing_app("screenrecorder");

	uint64_t video_track_id = segment->AddVideoTrack(width, height, 1);
	mkvmuxer::VideoTrack *video_track = static_cast<mkvmuxer::VideoTrack *>(segment->GetTrackByNumber(video_track_id));
	video_track->SetStereoMode(0); // No 3D.
	video_track->set_codec_id("V_VP8");

	return true;
}

void WebmWriter::close() {
	if (writer) {
		segment->Finalize();
		delete segment;
		delete writer;
		writer = NULL;
		segment = NULL;
		fclose(file);
		file = NULL;
	}
}

bool WebmWriter::write_frame(uint8_t *data, size_t size, int64_t timestamp, bool is_keyframe) {
	return segment->AddFrame(data, size, 1, timestamp * frame_ns, is_keyframe);
}

bool WebmWriter::mux_audio_video(const char *audio_filename, const char *video_filename, const char *filename) {
	// MUXER
	mkvmuxer::MkvWriter mux_writer;
	if (!mux_writer.Open(filename)) {
		dmLogError("Filename is invalid or error while opening.");
		return false;
	}
	mkvmuxer::Segment muxer_segment;
	if (!muxer_segment.Init(&mux_writer)) {
		dmLogError("Could not initialize muxer segment!");
		return false;
	}
	muxer_segment.AccurateClusterDuration(false);
	muxer_segment.UseFixedSizeClusterTimecode(false);
	muxer_segment.set_mode(mkvmuxer::Segment::kFile);
	muxer_segment.OutputCues(false);
	muxer_segment.GetSegmentInfo()->set_writing_app("screenrecorder");

	// VIDEO READER
	mkvparser::MkvReader video_reader;
	if (video_reader.Open(video_filename)) {
		dmLogError("Video filename is invalid or error while opening.");
		return false;
	}
	mkvparser::Segment* video_segment;
	long long ret = mkvparser::Segment::CreateInstance(&video_reader, 0, video_segment);
	if (ret) {
		dmLogError("Segment::CreateInstance() failed.");
		return false;
	}
	ret = video_segment->Load();
	if (ret < 0) {
		dmLogError("Video Segment::Load() failed.");
		return false;
	}
	const mkvparser::SegmentInfo *const video_segment_info = video_segment->GetInfo();
	if (video_segment_info == NULL) {
		dmLogError("Video Segment::GetInfo() failed.");
		return false;
	}
	const long long time_code_scale = video_segment_info->GetTimeCodeScale();
	muxer_segment.GetSegmentInfo()->set_timecode_scale(video_segment_info->GetTimeCodeScale());

	// Set Tracks element attributes
	const mkvparser::Tracks* const video_tracks = video_segment->GetTracks();
	if (video_tracks->GetTracksCount() == 0) {
		dmLogError("Video file has no tracks.");
		return false;
	}
	const mkvparser::Track* const video_track = video_tracks->GetTrackByIndex(0);
	if (video_track->GetType() != mkvparser::Track::kVideo) {
		dmLogError("Video file has no video track.");
		return false;
	}
	// Get the video track from the parser
	const mkvparser::VideoTrack *const p_video_track = static_cast<const mkvparser::VideoTrack*>(video_track);
	// Add the video track to the muxer
	uint64_t video_track_id = muxer_segment.AddVideoTrack(p_video_track->GetWidth(), p_video_track->GetHeight(), 0);
	if (!video_track_id) {
		dmLogError("Could not add video track.");
		return false;
	}
	mkvmuxer::VideoTrack* const muxer_video_track = static_cast<mkvmuxer::VideoTrack*>(muxer_segment.GetTrackByNumber(video_track_id));
	if (!muxer_video_track) {
		dmLogError("Could not get video track.");
		return false;
	}
	muxer_video_track->set_codec_id(p_video_track->GetCodecId());
	const double rate = p_video_track->GetFrameRate();
	if (rate > 0.0) {
		muxer_video_track->set_frame_rate(rate);
		dmLogInfo("Frame rate: %f", rate);
		frame_ns = 1000000000ll / rate;
	}

	// AUDIO READER
	mkvparser::MkvReader audio_reader;
	if (audio_reader.Open(audio_filename)) {
		dmLogError("Audio filename is invalid or error while opening.");
		return false;
	}
	mkvparser::Segment *audio_segment;
	ret = mkvparser::Segment::CreateInstance(&audio_reader, 0, audio_segment);
	if (ret) {
		dmLogError("Audio Segment::CreateInstance() failed.");
		return false;
	}
	ret = audio_segment->Load();
	if (ret < 0) {
		dmLogError("Audio segment load failed.");
		return false;
	}

	// Set Tracks element attributes
	const mkvparser::Tracks *const audio_tracks = audio_segment->GetTracks();
	if (audio_tracks->GetTracksCount() == 0) {
		dmLogError("Audio file has no tracks.");
		return false;
	}
	const mkvparser::Track *const audio_track = audio_tracks->GetTrackByIndex(0);
	if (audio_track->GetType() != mkvparser::Track::kAudio) {
		dmLogError("Audio file has no audio track.");
		return false;
	}
	// Get the audio track from the parser
	const mkvparser::AudioTrack *const p_audio_track = static_cast<const mkvparser::AudioTrack*>(audio_track);
	// Add the audio track to the muxer
	uint64_t audio_track_id = muxer_segment.AddAudioTrack(p_audio_track->GetSamplingRate(), p_audio_track->GetChannels(), 0);
	if (!audio_track_id) {
		dmLogError("Could not add audio track.");
		return false;
	}
	mkvmuxer::AudioTrack *const muxer_audio_track = static_cast<mkvmuxer::AudioTrack*>(muxer_segment.GetTrackByNumber(audio_track_id));
	if (!muxer_audio_track) {
		dmLogError("Could not get audio track.");
		return false;
	}
	muxer_audio_track->set_codec_id(p_audio_track->GetCodecId());
	const long long bit_depth = p_audio_track->GetBitDepth();
	if (bit_depth > 0) muxer_audio_track->set_bit_depth(bit_depth);
	if (p_audio_track->GetCodecDelay()) muxer_audio_track->set_codec_delay(p_audio_track->GetCodecDelay());
	if (p_audio_track->GetSeekPreRoll()) muxer_audio_track->set_seek_pre_roll(p_audio_track->GetSeekPreRoll());
	size_t private_size;
	const unsigned char *const private_data = p_audio_track->GetCodecPrivate(private_size);
	if (private_size > 0) {
		if (!muxer_audio_track->SetCodecPrivate(private_data, private_size)) {
			dmLogError("Could not add audio private data.");
		}
	}

	// Write clusters
	unsigned char *data = NULL;
	long data_len = 0;

	const mkvparser::Cluster *audio_cluster = NULL;
	const mkvparser::Cluster *video_cluster = NULL;
	const mkvparser::BlockEntry *audio_block_entry = NULL;
	const mkvparser::BlockEntry *video_block_entry = NULL;
	const mkvparser::Block *audio_block = NULL;
	const mkvparser::Block *video_block = NULL;

	long long audio_loop_start_time = 0;
	bool has_blocks = true;
	while (has_blocks) {
		if (video_block == NULL) {
			video_block = get_block(video_segment, &video_cluster, &video_block_entry);
			if (video_block == NULL) {
				has_blocks = false;
				break;
			}
		}
		const long long video_time = video_block->GetTime(video_cluster);

		if (audio_block == NULL) {
			audio_block = get_block(audio_segment, &audio_cluster, &audio_block_entry);
			if (audio_block == NULL && audio_cluster != NULL) {
				// Audio track has ended. Loop from the beginning.
				audio_cluster = NULL;
				audio_block_entry = NULL;
				audio_block = get_block(audio_segment, &audio_cluster, &audio_block_entry);
				audio_loop_start_time = video_time;
			}
		}

		const long long audio_time = audio_block != NULL ? (audio_loop_start_time + audio_block->GetTime(audio_cluster)) : 0;

		if (audio_block != NULL && audio_time <= video_time) {
			if (!write_block(&muxer_segment, &audio_reader, audio_block, audio_track_id, audio_time, &data, &data_len)) {
				dmLogInfo("Writing audio block failed");
				break;
			}
			audio_block = NULL;
		} else if (audio_time >= video_time || audio_block == NULL) {
			if (!write_block(&muxer_segment, &video_reader, video_block, video_track_id, video_time, &data, &data_len)) {
				dmLogInfo("Writing video block failed");
				break;
			}
			video_block = NULL;
		}
	}

	const double input_duration = static_cast<double>(video_segment_info->GetDuration()) / time_code_scale;
	muxer_segment.set_duration(input_duration);

	if (!muxer_segment.Finalize()) {
		dmLogError("Finalization of segment failed.");
		return false;
	}

	audio_reader.Close();
	video_reader.Close();
	mux_writer.Close();

	delete []data;
	
	return true;
}

const mkvparser::Block *WebmWriter::get_block(mkvparser::Segment *segment, const mkvparser::Cluster **cluster, const mkvparser::BlockEntry **block_entry) {
	const mkvparser::Block *block = NULL;
	if (*cluster == NULL) {
		*cluster = segment->GetFirst();
	}
	if (!(*cluster)->EOS()) {
		if (*block_entry == NULL) {
			if ((*cluster)->GetFirst(*block_entry)) {
				dmLogError("Failed to get the first block entry of a cluster.");
				return NULL;
			}
		} else {
			if ((*cluster)->GetNext(*block_entry, *block_entry)) {
				dmLogError("Failed to get next block of a cluster.");
				return NULL;
			}
		}
		if (*block_entry != NULL && !(*block_entry)->EOS()) {
			block = (*block_entry)->GetBlock();
		} else {
			*cluster = segment->GetNext(*cluster);
			block = get_block(segment, cluster, block_entry);
		}
	}
	return block;
}

bool WebmWriter::write_block(mkvmuxer::Segment *muxer_segment, mkvparser::MkvReader *reader, const mkvparser::Block *block, uint64_t track_number, long long time_ns, unsigned char **data, long *data_len) {
	const int frame_count = block->GetFrameCount();
	for (int i = 0; i < frame_count; ++i) {
		const mkvparser::Block::Frame &frame = block->GetFrame(i);
		if (frame.len > *data_len) {
			delete []*data;
			*data = new unsigned char[frame.len];
			if (!*data) {
				dmLogError("Could not allocate memory for block data.");
				return false;
			}
			*data_len = frame.len;
		}
		if (frame.Read(reader, *data)) {
			dmLogError("Could not read block frame.");
			return false;
		}
		mkvmuxer::Frame muxer_frame;
		if (!muxer_frame.Init(*data, frame.len)) {
			dmLogError("Could not init muxer frame.");
			return false;
		}
		muxer_frame.set_track_number(track_number);
		if (block->GetDiscardPadding()) muxer_frame.set_discard_padding(block->GetDiscardPadding());
		muxer_frame.set_timestamp(time_ns);
		muxer_frame.set_is_key(block->IsKey());
		if (!muxer_segment->AddGenericFrame(&muxer_frame)) {
			dmLogError("Could not add frame.");
			return false;
		}
	}
	return true;
}

#endif