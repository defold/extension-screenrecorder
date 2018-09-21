#ifndef circular_buffer_h
#define circular_buffer_h

#include <stdint.h>
#include <stddef.h>

class CircularBuffer {
private:
	uint8_t *buffer;
	uint8_t **pointers;
	size_t *sizes;
	int64_t *timestamps;
	bool *is_keyframes;
	size_t buffer_size;
	uint32_t count;
	uint32_t index;
	bool is_full;
	uint32_t index_start;
	uint8_t *current_pointer;
public:
	CircularBuffer();
	~CircularBuffer();
	bool init(size_t buffer_size, uint32_t count);
	bool add_frame(uint8_t *data, size_t size, int64_t timestamp, bool is_keyframe);
	bool get_frame(uint8_t **data, size_t *size, int64_t *timestamp, bool *is_keyframe, uint32_t *frame_index);
};

#endif