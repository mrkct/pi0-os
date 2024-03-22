#include <kernel/vfs/pipe.h>
#include <kernel/memory/kheap.h>
#include <kernel/kprintf.h>

#include <kernel/task/scheduler.h>


namespace kernel {

static constexpr uint32_t PIPE_BUFFER_SIZE = 1024;

struct PipeBuffer {
    uint32_t refcount;

    uint32_t rd, wr;
    uint8_t data[PIPE_BUFFER_SIZE + 1];

    uint32_t size() const
    {
        if (wr < rd) {
            return sizeof(data) - (rd - wr) - 1;
        } else {
            return wr - rd;
        }
    }

    uint32_t available() const { return sizeof(data) - 1 - size(); }
};

static Error pipe_read(File &file, uint64_t offset, uint8_t *out_buf, uint32_t size, uint32_t &bytes_read)
{
    (void) offset;
    auto *pipe = static_cast<PipeBuffer*>(file.opaque);

    uint32_t available = pipe->size();
    uint32_t to_read = size < available ? size : available;

    for (uint32_t i = 0; i < to_read; ++i) {
        out_buf[i] = pipe->data[(pipe->rd + i) % sizeof(pipe->data)];
    }

    pipe->rd = (pipe->rd + to_read) % sizeof(pipe->data);
    bytes_read = to_read;

    return Success;
}

static Error pipe_write(File &file, uint64_t offset, uint8_t const* buf, uint32_t size, uint32_t &bytes_written)
{
    (void) offset;
    auto *pipe = static_cast<PipeBuffer*>(file.opaque);

    uint32_t space_available = pipe->available();
    uint32_t to_write = (size < space_available) ? size : space_available;

    for (uint32_t i = 0; i < to_write; ++i) {
        pipe->data[(pipe->wr + i) % sizeof(pipe->data)] = buf[i];
    }

    pipe->wr = (pipe->wr + to_write) % sizeof(pipe->data);
    bytes_written = to_write;

    return Success;
}

static Error pipe_close(File &file)
{
    auto *pipe = static_cast<PipeBuffer*>(file.opaque);
    pipe->refcount--;
    if (pipe->refcount == 0)
        kfree(pipe);

    return Success;
}

Error create_pipe(FileCustody &out_readend, FileCustody &out_writeend)
{
    File *files;
    TRY(kmalloc(2 * sizeof(File), files));

    PipeBuffer *buf;
    if (auto err = kmalloc(sizeof(PipeBuffer), buf); !err.is_success()) {
        kfree(files);
        return err;
    }

    *buf = PipeBuffer {
        .refcount = 2,
        .rd = 0,
        .wr = 0,
        .data = {}
    };

    files[0] = File {
        .fs = NULL,
        .refcount = 1,
        .filetype = api::Pipe,
        .size = 0,
        .opaque = buf,

        .read = pipe_read,
        .write = NULL,
        .seek = NULL,
        .close = pipe_close
    };

    files[1] = File {
        .fs = NULL,
        .refcount = 1,
        .filetype = api::Pipe,
        .size = 0,
        .opaque = buf,

        .read = NULL,
        .write = pipe_write,
        .seek = NULL,
        .close = pipe_close
    };

    out_readend = FileCustody {
        .file = &files[0],
        .flags = api::OPEN_FLAG_READ,
        .seek_position = 0
    };
    out_writeend = FileCustody {
        .file = &files[1],
        .flags = api::OPEN_FLAG_WRITE,
        .seek_position = 0
    };

    return Success;
}

}
