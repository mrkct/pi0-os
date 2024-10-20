#pragma once

#include <kernel/base.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef uint64_t InodeIdentifier;
struct Inode;
struct Filesystem;

enum class InodeType {
    FIFO,
    CharacterDevice,
    BlockDevice,
    Directory,
    RegularFile,
    SymbolicLink,
    Socket,
};

struct InodeOps {
    int (*stat)(Inode *self, struct stat *st);
};

struct InodeFileOps {
    int64_t (*read)(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
    int64_t (*write)(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
    int32_t (*ioctl)(Inode *self, uint32_t request, void *argp);
    uint64_t (*seek)(Inode *self, uint64_t current, int whence, int32_t offset);
};

struct InodeDirOps {
    int (*lookup)(Inode *self, const char *name, Inode *out_entry);
    int (*create)(Inode *self, const char *name, InodeType type, Inode **out_inode);
    int (*mkdir)(Inode *self, const char *name);
    int (*rmdir)(Inode *self, const char *name);
    int (*unlink)(Inode *self, const char *name);
};

struct Inode {
    int refcount;

    InodeType type;
    InodeIdentifier identifier;
    Filesystem *filesystem;

    mode_t mode;
    uid_t uid;
    uint64_t size;
    struct timespec access_time;
    struct timespec creation_time;
    struct timespec modification_time;

    void *opaque;

    InodeOps const *ops;
    union {
        InodeFileOps const *file_ops;
        InodeDirOps const *dir_ops;
    };
};

struct DirectoryEntry {
    char name[256];
    Inode inode;
};

struct FilesystemOps {
    /**
     * Called once when the filesystem is mounted.
     * The filesystem must return back the root inode (closed)
     */
    int (*on_mount)(Filesystem*, Inode*);

    int (*open_inode)(Filesystem*, Inode*);
    int (*close_inode)(Filesystem*, Inode*);
};

struct InodeCache {
    struct Entry {
        INTRUSIVE_LINKED_LIST_HEADER(InodeCache::Entry);

        InodeIdentifier identifier;
        Inode *inode;
    };
    IntrusiveLinkedList<InodeCache::Entry> list;
};

struct Filesystem {
    FilesystemOps const *ops;
    InodeCache icache;
    InodeIdentifier root;
    void *opaque;
};

static inline int inode_cache_init(InodeCache *icache) { icache->list = {nullptr, nullptr}; return 0; }

static inline void inode_cache_free(InodeCache*) { TODO(); }

uint64_t default_checked_seek(uint64_t filesize, uint64_t current, int whence, int32_t offset);
