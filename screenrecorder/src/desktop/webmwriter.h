#ifndef webmwriter_h
#define webmwriter_h

#include <webm/mkvmuxer/mkvmuxer.h>
#include <webm/mkvmuxer/mkvmuxerutil.h>
#include <webm/mkvmuxer/mkvwriter.h>
#include <webm/mkvparser/mkvreader.h>

class WebmWriter {
private:
	FILE *file;
	int64_t frame_ns;
	mkvmuxer::MkvWriter *writer;
	mkvmuxer::Segment *segment;
	const mkvparser::Block *get_block(mkvparser::Segment *parser_segment, const mkvparser::Cluster **cluster, const mkvparser::BlockEntry **block_entry);
	bool write_block(mkvmuxer::Segment *muxer_segment, mkvparser::MkvReader *reader, const mkvparser::Block *block, uint64_t track_number, long long time_ns, unsigned char **data, long *data_len);
public:
	WebmWriter();
	~WebmWriter();
	bool open(const char *filename, int width, int height, int fps);
	void close();
	bool write_frame(uint8_t *data, size_t size, int64_t timestamp, bool is_keyframe);
	bool mux_audio_video(const char *audio_filename, const char *video_filename, const char *filename, char *error_message);
};

#endif
