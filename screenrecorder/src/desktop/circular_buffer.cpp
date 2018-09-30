#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_LINUX) || defined(DM_PLATFORM_WINDOWS)

#include <memory>

#include "circular_buffer.h"
#include "utils.h"

CircularBuffer::CircularBuffer() :
	buffer(NULL),
	pointers(NULL),
	sizes(NULL),
	timestamps(NULL),
	is_keyframes(NULL),
	buffer_size(0),
	count(0),
	index(0),
	is_full(false),
	index_start(0),
	current_pointer(NULL) {
	}

bool CircularBuffer::init(size_t buffer_size, uint32_t count) {
	this->buffer_size = buffer_size;
	this->count = count;
	buffer = new uint8_t[buffer_size];
	pointers = new uint8_t*[count];
	sizes = new size_t[count];
	timestamps = new int64_t[count];
	is_keyframes = new bool[count];
	current_pointer = buffer;
	if (buffer == NULL || pointers == NULL || sizes == NULL || timestamps == NULL || is_keyframes == NULL) {
		return false;
	}
	return true;
}

CircularBuffer::~CircularBuffer() {
	delete []buffer;
	delete []pointers;
	delete []sizes;
	delete []timestamps;
	delete []is_keyframes;
}

bool CircularBuffer::add_frame(uint8_t *data, size_t size, int64_t timestamp, bool is_keyframe) {
	uint8_t *destination = current_pointer;
	if (is_full) {
		// Advance tail.
		index_start = (index + 1) % count;
	}
	if (destination + size >= buffer + buffer_size) {
		//dmLogDebug("Buffer reset on index %d", index);
		// Reset to buffer start if out of buffer range.
		destination = buffer;
	}
	if (is_full) {
		// Discard any tail frames that overlap the new frame.
		while (
			(destination <= pointers[index_start] && destination + size >= pointers[index_start]) ||
			(destination >= pointers[index_start] && pointers[index_start] + sizes[index_start] >= destination)
		) {
			index_start = (index_start + 1) % count;
		}
	}
	memcpy(destination, data, size);
	current_pointer = destination + size;
	pointers[index] = destination;
	sizes[index] = size;
	timestamps[index] = timestamp;
	is_keyframes[index] = is_keyframe;
	++index;
	if (index == count) {
		index = 0;
		is_full = true;
	}
	return true;
}

bool CircularBuffer::get_frame(uint8_t **data, size_t *size, int64_t *timestamp, bool *is_keyframe, uint32_t *frame_index) {
	uint32_t i = (index_start + *frame_index) % count;
	// Exit when looped and returned back to index_start or reached current index in case of short video.
	if (*frame_index > 0 && ((is_full && i == index_start) || (!is_full && i == index))) {
		return false;
	}
	*data = pointers[i];
	*size = sizes[i];
	*timestamp = timestamps[i];
	*is_keyframe = is_keyframes[i];
	*frame_index += 1;
	return true;
}

#endif