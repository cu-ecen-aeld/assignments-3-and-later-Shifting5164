/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else

#include <string.h>

#endif

#include "aesd-circular-buffer.h"

uint8_t calc_pos_after(uint8_t current) {
    return ((++current) % (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED));
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset,
                                                                          size_t *entry_offset_byte_rtn) {
    /**
    * Done: implement per description
    */

    /* calc total size of buffer */
    int32_t iBuffEntries;
    if (buffer->full == true) {
        iBuffEntries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else {
        iBuffEntries = (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED + buffer->in_offs - buffer->out_offs) %
                       AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    /* Loop entries */
    int32_t i;

    /* Start with the current read position in the buffer and work from there until all entries are searched */
    uint8_t u8Entry = buffer->out_offs;

    /* Keep track of the current offsets based on the buffer u8Entry size */
    size_t iCurrOffset = 0;

    /* Loop al buffer entries until we searched all, beginning with `out_offs` until `iBuffEntries` */
    for (i = 0; i < iBuffEntries; i++) {
        /* Based on the entry size, could the offset be in the current entry, if not then keep trakc of it for
         * later usage*/
        if ((buffer->entry[u8Entry].size + iCurrOffset) <= char_offset) {
            iCurrOffset += buffer->entry[u8Entry].size;
        } else {
            /* return offset and buffer entry */
            *entry_offset_byte_rtn = char_offset - iCurrOffset;
            return &buffer->entry[u8Entry];
        }

        u8Entry = calc_pos_after(u8Entry);
    }

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry) {
    /**
    * DONE: implement per description
    */

    /* add item, overwrite whatever, fix index later */
    memcpy(&buffer->entry[buffer->in_offs], add_entry, sizeof(aesd_buffer_entry));

    /* advance in_offs unconditionally */
    buffer->in_offs = calc_pos_after(buffer->in_offs);

    if (buffer->full == true) {
        /* when (still) full, and we write, advance the out pointer and lose old data */
        /* out_offs and in_offs should have the same value */
        buffer->out_offs = calc_pos_after(buffer->out_offs);
    } else {
        /* when in_offs and out_offs overlap buffer is full, mark it*/
        if (buffer->in_offs == buffer->out_offs) {
            buffer->full = true;
        }
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer) {
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}