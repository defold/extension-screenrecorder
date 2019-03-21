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
import java.nio.ByteBuffer;

/**
 * Holds encoded video data in a circular buffer.
 * <p>
 * This is actually a pair of circular buffers, one for the raw data and one for the meta-data
 * (flags and PTS).
 * <p>
 * Not thread-safe.
 */
class CircularEncoderBuffer {
    private static final boolean EXTRA_DEBUG = true;
    // Raw data (e.g. AVC NAL units) held here.
    //
    // The MediaMuxer writeSampleData() function takes a ByteBuffer.  If it's a "direct"
    // ByteBuffer it'll access the data directly, if it's a regular ByteBuffer it'll use
    // JNI functions to access the backing byte[] (which, in the current VM, is done without
    // copying the data).
    //
    // It's much more convenient to work with a byte[], so we just wrap it with a ByteBuffer
    // as needed.  This is a bit awkward when we hit the edge of the buffer, but for that
    // we can just do an allocation and data copy (we know it happens at most once per file
    // save operation).
    private ByteBuffer data_buffer_wrapper;
    private byte[] data_buffer;

    // Meta-data held here.  We're using a collection of arrays, rather than an array of
    // objects with multiple fields, to minimize allocations and heap footprint.
    private int[] packet_flags;
    private long[] packet_pts_usec;
    private int[] packet_start;
    private int[] packet_length;

    // Data is added at head and removed from tail.  Head points to an empty node, so if
    // head==tail the list is empty.
    private int meta_head;
    private int meta_tail;

    // Allocates the circular buffers we use for encoded data and meta-data.
    CircularEncoderBuffer(int bitrate, int framerate, int duration) {
        // For the encoded data, we assume the encoded bit rate is close to what we request.
        //
        // There would be a minor performance advantage to using a power of two here, because
        // not all ARM CPUs support integer modulus.
        int data_buffer_size = bitrate * duration / 8;
        data_buffer = new byte[data_buffer_size];
        data_buffer_wrapper = ByteBuffer.wrap(data_buffer);

        // Meta-data is smaller than encoded data for non-trivial frames, so we over-allocate
        // a bit.  This should ensure that we drop packets because we ran out of (expensive)
        // data storage rather than (inexpensive) metadata storage.
        int meta_buffer_count = framerate * duration * 2;
        packet_flags = new int[meta_buffer_count];
        packet_pts_usec = new long[meta_buffer_count];
        packet_start = new int[meta_buffer_count];
        packet_length = new int[meta_buffer_count];
    }

    // Computes the amount of time spanned by the buffered data, based on the presentation time stamps.
    /*long computeTimeSpanUsec() {
        final int metaLen = packet_start.length;
        if (meta_head == meta_tail) {
            // empty list
            return 0;
        }
        // head points to the next available node, so grab the previous one
        int beforeHead = (meta_head + metaLen - 1) % metaLen;
        return packet_pts_usec[beforeHead] - packet_pts_usec[meta_tail];
    }*/

    /**
     * Adds a new encoded data packet to the buffer.
     *
     * @param buffer The data.  Set position() to the start offset and limit() to position+size.
     *     The position and limit may be altered by this method.
     * @param flags MediaCodec.BufferInfo flags.
     * @param pts_usec Presentation time stamp, in microseconds.
     */
    void add(ByteBuffer buffer, int flags, long pts_usec) {
        int size = buffer.limit() - buffer.position();
        while (!has_enough_space(size)) {
            remove_tail();
        }

        final int dataLen = data_buffer.length;
        final int metaLen = packet_start.length;
        int packet_start = get_head_start();
        packet_flags[meta_head] = flags;
        packet_pts_usec[meta_head] = pts_usec;
        this.packet_start[meta_head] = packet_start;
        packet_length[meta_head] = size;

        // Copy the data in.  Take care if it gets split in half.
        if (packet_start + size < dataLen) {
            // one chunk
            buffer.get(data_buffer, packet_start, size);
        } else {
            // two chunks
            int first_size = dataLen - packet_start;
            buffer.get(data_buffer, packet_start, first_size);
            buffer.get(data_buffer, 0, size - first_size);
        }

        meta_head = (meta_head + 1) % metaLen;

        if (EXTRA_DEBUG) {
            // The head packet is the next-available spot.
            packet_flags[meta_head] = 0x77aaccff;
            packet_pts_usec[meta_head] = -1000000000L;
            this.packet_start[meta_head] = -100000;
            packet_length[meta_head] = Integer.MAX_VALUE;
        }
    }

    /**
     * Returns the index of the oldest sync frame.  Valid until the next add().
     * <p>
     * When sending output to a MediaMuxer, start here.
     */
    int get_first_index() {
        int index = meta_tail;
        while (index != meta_head) {
            if ((packet_flags[index] & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
                break;
            }
            index = (index + 1) % packet_start.length;
        }
        if (index == meta_head) {
			Utils.log("CircularEncoder: could not find sync frame in buffer");
            index = -1;
        }
        return index;
    }

    // Returns the index of the next packet, or -1 if we've reached the end.
    int get_next_index(int index) {
        int next = (index + 1) % packet_start.length;
        if (next == meta_head) {
            next = -1;
        }
        return next;
    }

    /**
     * Returns a reference to a "direct" ByteBuffer with the data, and fills in the
     * BufferInfo.
     * <p>
     * The caller must not modify the contents of the returned ByteBuffer.  Altering
     * the position and limit is allowed.
     */
    ByteBuffer get_chunk(int index, MediaCodec.BufferInfo info) {
        final int data_length = data_buffer.length;
        int packet_start = this.packet_start[index];
        int length = packet_length[index];

        info.flags = packet_flags[index];
        info.offset = packet_start;
        info.presentationTimeUs = packet_pts_usec[index];
        info.size = length;

        if (packet_start + length <= data_length) {
            // one chunk; return full buffer to avoid copying data
            return data_buffer_wrapper;
        } else {
            // two chunks
            ByteBuffer temp_buffer = ByteBuffer.allocateDirect(length);
            int first_size = data_length - packet_start;
            temp_buffer.put(data_buffer, this.packet_start[index], first_size);
            temp_buffer.put(data_buffer, 0, length - first_size);
            info.offset = 0;
            return temp_buffer;
        }
    }

    /**
     * Computes the data buffer offset for the next place to store data.
     * <p>
     * Equal to the start of the previous packet's data plus the previous packet's length.
     */
    private int get_head_start() {
        if (meta_head == meta_tail) {
            return 0;
        }
        int beforeHead = (meta_head + packet_start.length - 1) % packet_start.length;
        return (packet_start[beforeHead] + packet_length[beforeHead] + 1) % data_buffer.length;
    }

    /**
     * Determines whether this is enough space to fit "size" bytes in the data buffer, and
     * one more packet in the meta-data buffer.
     *
     * @return True if there is enough space to add without removing anything.
     */
    private boolean has_enough_space(int size) {
        final int data_length = data_buffer.length;
        final int meta_length = packet_start.length;
        if (size > data_length) {
			Utils.log("CircularEncoder: enormous packet: " + size + " vs. buffer " + data_length + ".");
			return false;
        }
        if (meta_head == meta_tail) {
            // Empty list.
            return true;
        }
        // Make sure we can advance head without stepping on the tail.
        int next_head = (meta_head + 1) % meta_length;
        if (next_head == meta_tail) {
			//Utils.log("CircularEncoder: ran out of metadata (head=" + meta_head + " tail=" + meta_tail +").");
            return false;
        }
        // Need the byte offset of the start of the "tail" packet, and the byte offset where
        // "head" will store its data.
        int head_start = get_head_start();
        int tail_start = packet_start[meta_tail];
        int free_space = (tail_start + data_length - head_start) % data_length;
		return size <= free_space;
	}

    // Removes the tail packet.
    private void remove_tail() {
        if (meta_head != meta_tail) {
			meta_tail = (meta_tail + 1) % packet_start.length;
        }
    }
}
