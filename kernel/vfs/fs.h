#pragma once

#include <kernel/base.h>
#include <kernel/drivers/device.h>


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
    Pipe,
};

struct InodeOps {
};

struct InodeFileOps {
    int64_t (*read)(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
    int64_t (*write)(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
    int32_t (*ioctl)(Inode *self, uint32_t request, void *argp);
    uint64_t (*seek)(Inode *self, uint64_t current, int whence, int32_t offset);
    int32_t (*poll)(Inode *self, uint32_t events, uint32_t *out_revents);
    int32_t (*mmap)(Inode *self, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags);
    int32_t (*istty)(Inode *self);
};

struct InodeDirOps {
    int (*lookup)(Inode *self, const char *name, Inode *out_entry);
    int (*create)(Inode *self, const char *name, InodeType type, Inode *out_inode);
    int (*unlink)(Inode *self, const char *name);
};

struct Inode {
    int refcount;

    InodeType type;
    InodeIdentifier identifier;
    Filesystem *filesystem;

    uint16_t devmajor, devminor;
    uint32_t mode;
    uint32_t uid, gid;
    uint64_t size;
    api::TimeSpec access_time;
    api::TimeSpec creation_time;
    api::TimeSpec modification_time;
    uint64_t blksize;

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

int icache_insert(InodeCache *icache, InodeIdentifier identifier, Inode *inode);

Inode *icache_lookup(InodeCache *icache, InodeIdentifier identifier);

static inline void inode_cache_free(InodeCache*) { TODO(); }

uint64_t default_checked_seek(uint64_t filesize, uint64_t current, int whence, int32_t offset);

Filesystem *fs_detect_and_create(BlockDevice&);

int32_t fs_inode_ioctl_not_supported(Inode*, uint32_t, void*);
uint64_t fs_inode_seek_not_supported(Inode*, uint64_t, int, int32_t);
int32_t fs_file_inode_poll_always_ready(Inode *self, uint32_t events, uint32_t *out_revents);
int32_t fs_file_inode_mmap_not_supported(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t);
int32_t fs_file_inode_istty_always_false(Inode*);
int fs_dir_inode_create_not_supported(Inode*, const char*, InodeType, Inode *);
int fs_dir_inode_mkdir_not_supported(Inode*, const char *);
int fs_dir_inode_rmdir_not_supported(Inode*, const char *);
int fs_dir_inode_unlink_not_supported(Inode*, const char *);
