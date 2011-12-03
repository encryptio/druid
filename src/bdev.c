#include "bdev.h"

#include <string.h>
#include <assert.h>

bool generic_read_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *into) {
    if ( len == 0 ) return true;

    uint64_t start_block = start/self->block_size;
    uint64_t end_block = (start+len-1)/self->block_size;
    uint64_t skip_bytes = start - start_block*self->block_size;

    assert(start_block < self->block_count);
    assert(  end_block < self->block_count);

    while ( len > 0 ) {
        assert(start_block < self->block_count);
        if ( start_block == end_block ) {
            if ( !self->read_block(self, start_block, self->generic_block_buffer) )
                return false;

            memcpy(into, self->generic_block_buffer+skip_bytes, len);
            return true;
        } else {
            if ( skip_bytes ) {
                if ( !self->read_block(self, start_block, self->generic_block_buffer) )
                    return false;

                uint64_t read_size = self->block_size - skip_bytes;
                assert( len > read_size );

                memcpy(into, self->generic_block_buffer+skip_bytes, read_size);
                len -= read_size;
                into += read_size;
                start_block++;
                skip_bytes = 0;
            } else {
                assert( len >= self->block_size );
                if ( !self->read_block(self, start_block, into) )
                    return false;

                len -= self->block_size;
                into += self->block_size;
                start_block++;
            }
        }
    }

    return true; // len == 0
}

bool generic_write_bytes(struct bdev *self, uint64_t start, uint64_t len, uint8_t *from) {
    if ( len == 0 ) return true;

    uint64_t start_block = start/self->block_size;
    uint64_t end_block = (start+len-1)/self->block_size;
    uint64_t skip_bytes = start - start_block*self->block_size;

    assert(start_block < self->block_count);
    assert(  end_block < self->block_count);

    while ( len > 0 ) {
        assert(start_block < self->block_count);
        if ( skip_bytes || start_block == end_block ) {
            if ( !self->read_block(self, start_block, self->generic_block_buffer) )
                return false;

            uint64_t copy = self->block_size - skip_bytes;
            if ( copy > len ) copy = len;
            memcpy(self->generic_block_buffer+skip_bytes, from, copy);
            len -= copy;
            from += copy;
            skip_bytes = 0;

            if ( !self->write_block(self, start_block, self->generic_block_buffer) )
                return false;

            start_block++;

        } else {
            assert(len >= self->block_size);

            if ( !self->write_block(self, start_block, from) )
                return false;

            from += self->block_size;
            len -= self->block_size;
            start_block++;
        }
    }

    return true; // len == 0
}

