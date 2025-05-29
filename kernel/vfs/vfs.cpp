#include <dirent.h>
#include "vfs.h"
#include "fs.h"

#include <kernel/scheduler.h>

// #define LOG_ENABLED
#define LOG_TAG "VFS"
#include <kernel/log.h>

/**
 * 
 * ## The Inode Cache (icache)
 * The inode cache is where all the currently open inodes are stored.
 * 
 * Every entry in it must be unique and opened by at least 1 process.
 * It should not be possible to get an inode from the icache that
 * has a refcount of 0, or that it is not currently open.
 * 
 * If an inode is returned from the icache, it can be assumed that the
 * inode's open function has been previously called and it was successful
 */


struct MountPoint {
    INTRUSIVE_LINKED_LIST_HEADER(MountPoint);
    Filesystem *fs;
    char *path;
    uint32_t path_skip;
};

static IntrusiveLinkedList<MountPoint> s_mountpoints;


static int open_inode(Inode *inode)
{
    kassert(inode != nullptr);

    if (inode->refcount > 0) {
        inode->refcount++;
        LOGI("Opening inode %" PRIu64 " (increasing refcount to %d)", inode->identifier, inode->refcount);
        return 0;
    }

    auto *fs = inode->filesystem;
    kassert(fs != nullptr);
    LOGI("Opening inode %" PRIu64, inode->identifier);
    int rc = fs->ops->open_inode(fs, inode);
    if (rc != 0) {
        LOGE("Failed to open inode %" PRIu64 ": %d", inode->identifier, rc);
        return rc;
    }

    inode->refcount = 1;
    return 0;
}

static void close_inode(Inode *inode)
{
    if (inode == nullptr)
        return;

    kassert(inode->refcount > 0);
    if (inode->refcount > 1) {
        inode->refcount--;
        return;
    }

    inode->filesystem->ops->close_inode(inode->filesystem, inode);
}

static void free_custody(FileCustody *custody)
{
    if (custody == nullptr)
        return;
    
    close_inode(custody->inode);
    free(custody);
}

static int alloc_custody(Inode *inode, uint32_t flags, FileCustody **out_custody)
{
    auto *custody = static_cast<FileCustody*>(malloc(sizeof(FileCustody)));
    if (!custody)
        return -ERR_NOMEM;
    
    custody->inode = inode;
    custody->flags = flags;
    custody->offset = 0;
    *out_custody = custody;
    return 0;
}

static int lookup_inode(Inode *parent, const char *name, Inode **out_inode)
{
    int rc;
    Inode temp = {};

    LOGI("Looking up inode %s in %" PRIu64, name, parent->identifier);
    rc = parent->dir_ops->lookup(parent, name, &temp);
    if (rc != 0) {
        LOGE("Failed to lookup entry '%s' in inode %" PRIu64, name, parent->identifier);
        return rc;
    }
    LOGI("Found");

    *out_inode = icache_lookup(&parent->filesystem->icache, temp.identifier);
    if (*out_inode != nullptr)
        return open_inode(*out_inode);

    *out_inode = (Inode*) malloc(sizeof(Inode));
    if (*out_inode == nullptr)
        return -ERR_NOMEM;
        
    **out_inode = temp;
    (*out_inode)->refcount = 0;
    rc = open_inode(*out_inode);
    if (rc != 0) {
        free(*out_inode);
        *out_inode = nullptr;
        return rc;
    }
    
    rc = icache_insert(&parent->filesystem->icache, temp.identifier, *out_inode);
    if (rc != 0) {
        close_inode(*out_inode);
        free(*out_inode);
        *out_inode = nullptr;
        return rc;
    }

    return 0;
}

/**
 * Traverses a path in a filesystem.
 * The path needs to be relative to the filesystem and already canonicalized.
 * 
 * If the lookup is successful, the found inode and its parent will be stored
 * in out_parent and out_inode respectively. The pointers are owned by the
 * filesystem's inode cache.
 * 
 * If the lookup failed on the last component of the path, and the inode
 * for the component before that last one is a directory, out_inode will be
 * not be set but out_parent will be set to the parent of the last component.
 * 
 * In all other cases, both out_parent and out_inode will be set to nullptr.
 * 
 * @param fs The filesystem to lookup in
 * @param fs_relative_canonicalized_path The path to lookup, relative to the filesystem
 * @param out_parent The parent inode of the inode we're looking for
 * @param out_inode The inode we're looking for
 * 
 * @return 0 on success, -errno on failure
 * 
*/
static int traverse_in_fs(Filesystem *fs, const char *fs_relative_canonicalized_path, Inode **out_parent, Inode **out_inode)
{
    const char *path = fs_relative_canonicalized_path;
    int rc = 0;

    Inode *parent = nullptr;
    Inode *inode = icache_lookup(&fs->icache, fs->root);
    if (inode == nullptr) {
        LOGE("root inode %lu not found", fs->root);
        return -ERR_NOENT;
    }

    // The loop below assumes that the previous inode was already opened
    rc = open_inode(inode);
    if (rc != 0) {
        LOGE("failed to open root inode");
        return rc;
    }

    LOGD("traversing path '%s'", fs_relative_canonicalized_path);
    while (rc == 0 && *path) {
        close_inode(parent);
        parent = inode;
        inode = nullptr;

        if (parent->type != InodeType::Directory) {
            LOGE("parent inode is not a directory");
            rc = -ERR_NOTDIR;
            break;
        }

        LOGD("lookup for '%s'", path);
        rc = lookup_inode(parent, path, &inode);
        if (rc != 0) {
            rc = -ERR_NOENT;
            // do not 'break', we want to know if this was the last component below
        }

        path += strlen(path) + 1;
    }

    if (rc == 0) {
        LOGI("lookup successful");
        *out_parent = parent;
        *out_inode = inode;
    } else if (rc == -ERR_NOENT && parent->type == InodeType::Directory && *path == '\0') {
        LOGE("lookup failed at the last component");
        *out_parent = parent;
        *out_inode = nullptr;
    } else {
        LOGE("lookup failed");
        close_inode(parent);
        *out_parent = nullptr;
        *out_inode = nullptr;
    }

    return rc;
}

static int traverse(const char *path, Inode **out_parent, Inode **out_inode)
{
    int rc;
    char *cpath = canonicalize_path(path);
    if (cpath == nullptr)
        return -ERR_NOMEM;

    LOGI("Lookup for '%s' (canonicalized to: '%s')", path, log_canonicalized_path(cpath));

    // NOTE: This assumes that the paths in the mountpoints are ordered by length descending
    auto *mp = s_mountpoints.find_first([&](MountPoint *mp) {
        return canonicalized_path_startswith(cpath, mp->path);
    });
    if (mp == nullptr) {
        LOGW("No mountpoint found");
        rc = -ERR_NOENT;
    } else {
        LOGI("Found mountpoint at '%s'", log_canonicalized_path(mp->path));
        rc = traverse_in_fs(mp->fs, cpath + mp->path_skip, out_parent, out_inode);
    }

    free(cpath);
    return rc;
}

static void inode_stat(Inode *inode, api::Stat *out_stat)
{
    *out_stat = (api::Stat) {
        .st_dev = ((uint32_t) inode->devmajor << 8) | inode->devminor,
        .st_ino = inode->identifier,
        .st_mode = 0666,
        .st_nlink = (uint32_t) inode->refcount,
        .st_uid = inode->uid,
        .st_gid = inode->gid,
        .st_rdev = 0,
        .st_size = inode->size,
        .atim = inode->access_time,
        .mtim = inode->modification_time,
        .ctim = inode->creation_time,
        .st_blksize = inode->blksize,
        .st_blocks = round_up(inode->size, inode->blksize) / inode->blksize,
    };

    switch (inode->type) {
    case InodeType::RegularFile:
        out_stat->st_mode |= SF_IFREG;
        break;
    case InodeType::Directory:
        out_stat->st_mode |= SF_IFDIR;
        break;
    case InodeType::Pipe:
        out_stat->st_mode |= SF_IFIFO;
        break;
    case InodeType::CharacterDevice:
        out_stat->st_mode |= SF_IFCHR;
        break;
    case InodeType::BlockDevice:
        out_stat->st_mode |= SF_IFBLK;
        break;
    case InodeType::Socket:
        out_stat->st_mode |= SF_IFSOCK;
        break;
    case InodeType::FIFO:
        out_stat->st_mode |= SF_IFIFO;
        break;
    case InodeType::SymbolicLink:
        out_stat->st_mode |= SF_IFLNK;
        break;
    default:
        LOGE("Unknown inode type %d", inode->type);
        out_stat->st_mode |= SF_IFREG;
        break;
    }
}

static int create_inode(Inode *parent, const char *name, InodeType type, Inode **out_inode)
{
    int rc;
    Inode temp = {};

    LOGI("Creating inode '%s' in %" PRIu64, name, parent->identifier);
    rc = parent->dir_ops->create(parent, name, type, &temp);
    if (rc != 0) {
        LOGE("Failed to create inode '%s' in %" PRIu64 ": %d", name, parent->identifier, rc);
        return rc;
    }
    
    *out_inode = icache_lookup(&parent->filesystem->icache, temp.identifier);
    kassert(*out_inode == nullptr);

    *out_inode = (Inode*) malloc(sizeof(Inode));
    if (*out_inode == nullptr)
        return -ERR_NOMEM;
        
    **out_inode = temp;
    (*out_inode)->refcount = 0;
    rc = open_inode(*out_inode);
    if (rc != 0) {
        free(*out_inode);
        *out_inode = nullptr;
        return rc;
    }
    
    rc = icache_insert(&parent->filesystem->icache, temp.identifier, *out_inode);
    if (rc != 0) {
        close_inode(*out_inode);
        free(*out_inode);
        *out_inode = nullptr;
        return rc;
    }

    return 0;
}

int vfs_open(const char *path, uint32_t flags, FileCustody **out_custody)
{
    int rc = 0;
    Inode *parent = nullptr;
    Inode *inode = nullptr;

    *out_custody = nullptr;
    rc = traverse(path, &parent, &inode);

    if (rc != 0 && parent != nullptr && (flags & OF_CREATE)) {
        kassert(parent != nullptr);
        kassert(parent->type == InodeType::Directory);

        const char *filename = strrchr(path, '/');
        kassert(filename != nullptr);
        filename += 1;

        rc = create_inode(parent, filename, InodeType::RegularFile, &inode);
    } else if (rc == 0) {
        auto *fs = inode->filesystem;
        kassert(fs != nullptr);
        kassert(fs->ops != nullptr);
        kassert(fs->ops->open_inode != nullptr);
    }

    if (rc == 0 && inode->type == InodeType::Directory && (flags & OF_DIRECTORY) == 0)
        rc = -ERR_ISDIR;

    if (rc == 0)
        rc = alloc_custody(inode, flags, out_custody);

    if (rc != 0) {
        close_inode(parent);
        close_inode(inode);
        free_custody(*out_custody);
    }

    LOGD("vfs_open(%s) -> %d (%s)", path, rc, strerror(rc));
    return rc;
}

int vfs_open(const char *workdir, const char *path, uint32_t flags, FileCustody **out_custody)
{
    if (path_is_absolute(path))
        return vfs_open(path, flags, out_custody);
    
    char *cpath = pathjoin(workdir, path);
    if (cpath == nullptr) {
        LOGE("Failed to allocate memory for path join");
        return -ERR_NOMEM;
    }
    
    int rc = vfs_open(cpath, flags, out_custody);
    free(cpath);

    return rc;
}

int vfs_mount(const char *path, Filesystem &fs)
{
    int rc = 0;
    Inode *root_inode = (Inode*) malloc(sizeof(Inode));
    char *cpath = canonicalize_path(path);
    auto *mp = (MountPoint*) malloc(sizeof(MountPoint));
    if (cpath == nullptr || mp == nullptr || root_inode == nullptr) {
        free(cpath);
        free(mp);
        free(root_inode);
        return -ERR_NOMEM;
    }
    
    mp->path = cpath;
    mp->fs = &fs;
    mp->path_skip = canonicalized_path_strlen(cpath) + 1;
    LOGD("Mountpoint %s ('%s') has path skip %" PRIu32, path, log_canonicalized_path(mp->path), mp->path_skip);

    auto *after = s_mountpoints.find_first([&](MountPoint *mp) {
        return canonicalized_path_strlen(mp->path) < canonicalized_path_strlen(cpath);
    });
    if (after) {
        LOGD("add before '%s'", log_canonicalized_path(after->path));
        s_mountpoints.append_before(mp, after);
    } else {
        LOGD("add to head");
        s_mountpoints.add(mp);
    }

    fs.ops->on_mount(&fs, root_inode);
    rc = open_inode(root_inode);
    kassert(rc == 0); // TODO: handle this error
    icache_insert(&fs.icache, root_inode->identifier, root_inode);


    s_mountpoints.foreach([&](MountPoint *mp) {
        LOGD("Mountpoint '%s'", log_canonicalized_path(mp->path));
    });

    return rc;
}

ssize_t vfs_read(FileCustody *custody, uint8_t *buffer, uint32_t size)
{
    bool is_dir = custody->inode->type == InodeType::Directory;

    if (is_dir && (custody->flags & OF_DIRECTORY) == 0)
        return -ERR_ISDIR;
    
    if ((custody->flags & OF_ACCMODE) == OF_WRONLY) {
        LOGI("vfs_read: denied because custody was opened with WRONLY flag (flags: %" PRIu32 ")", custody->flags);
        return -ERR_PERM;
    }

    if ((custody->flags & OF_NONBLOCK) == 0 && !is_dir) {
        uint32_t events = 0;
        while (vfs_poll(custody, F_POLLIN, &events) == 0 && (events & F_POLLIN) == 0) {
            sys$yield();
        }
    }

    LOGI("vfs_read(%" PRIu32 " bytes, custody offset @ %" PRIu64 ")", size, custody->offset);
    auto *inode = custody->inode;
    ssize_t rc = 0;
    if (!is_dir) {
        rc = inode->file_ops->read(inode, custody->offset, buffer, size);
    } else {
        rc = inode->dir_ops->getdents(inode, custody->offset, (struct dirent*) buffer, size / sizeof(struct dirent));
    }
    if (rc > 0)
        vfs_seek(custody, SEEK_CUR, rc);

    return rc;
}

ssize_t vfs_write(FileCustody *custody, uint8_t const *buffer, uint32_t size)
{
    if (custody->inode->type == InodeType::Directory)
        return -ERR_INVAL;

    if ((custody->flags & OF_ACCMODE) == OF_RDONLY) {
        LOGI("vfs_write: denied because custody was opened with RDONLY flag (flags: %" PRIu32 ")", custody->flags);
        return -ERR_PERM;
    }

    if ((custody->flags & OF_NONBLOCK) == 0) {
        uint32_t events = 0;
        while (vfs_poll(custody, F_POLLOUT, &events) == 0 && (events & F_POLLOUT) == 0) {
            sys$yield();
        }
    }

    auto *inode = custody->inode;
    ssize_t rc = inode->file_ops->write(inode, custody->offset, buffer, size);
    if (rc > 0)
        vfs_seek(custody, SEEK_CUR, rc);

    return rc;
}

ssize_t vfs_seek(FileCustody *custody, int whence, int32_t offset)
{
    auto *inode = custody->inode;
    ssize_t new_seek = 0;
    
    if (custody->inode->type != InodeType::Directory) {
        new_seek = inode->file_ops->seek(inode, custody->offset, whence, offset);
    } else {
        /* Seeking with directories is weird and not well defined in Unix,
           also because dir inodes often don't have a proper 'size' in many fs.
           This is here because it makes things work, not because it's really "right" */
        new_seek = default_checked_seek(0x7fffffff, custody->offset, whence, offset);
    }
    
    if (new_seek < 0)
        return new_seek;
    custody->offset = new_seek;
    return custody->offset;
}

int vfs_close(FileCustody *custody)
{
    free_custody(custody);
    return 0;
}

int vfs_ioctl(FileCustody *custody, uint32_t ioctl, void *argp)
{
    if (custody->inode->type == InodeType::Directory)
        return -ERR_ISDIR;

    return custody->inode->file_ops->ioctl(custody->inode, ioctl, argp);
}

int vfs_stat(const char *path, api::Stat *stat)
{
    int rc;
    Inode *parent = nullptr;
    Inode *inode = nullptr;

    rc = traverse(path, &parent, &inode);

    if (rc != 0)
        inode_stat(parent, stat);

    close_inode(parent);
    close_inode(inode);
    return rc;
}

int vfs_fstat(FileCustody *custody, api::Stat *stat)
{
    inode_stat(custody->inode, stat);
    return 0;
}

FileCustody* vfs_duplicate(FileCustody *custody)
{
    auto *dup = (FileCustody*) malloc(sizeof(FileCustody));
    if (dup == nullptr)
        return nullptr;
    
    *dup = *custody;
    dup->inode->refcount++;

    return dup;
}

int vfs_create_pipe(FileCustody **out_sender_custody, FileCustody **out_receiver_custody)
{
    int rc = 0;
    FileCustody *sender = nullptr;
    FileCustody *receiver = nullptr;

    LOGD("Creating a new pipe");
    rc = vfs_open("/pipe/new", OF_CREATE, &sender);
    if (rc != 0) {
        LOGE("Failed to create pipe: %d", rc);
        goto cleanup;
    }

    receiver = vfs_duplicate(sender);
    if (receiver == nullptr) {
        LOGE("Failed to duplicate pipe");
        rc = -ERR_NOMEM;
        goto cleanup;
    }

    sender->flags = OF_WRONLY;
    receiver->flags = OF_RDONLY;
    *out_sender_custody = sender;
    *out_receiver_custody = receiver;
    
    return 0;

cleanup:
    free_custody(sender);
    free_custody(receiver);
    return rc;
}

int32_t vfs_poll(FileCustody *custody, uint32_t events, uint32_t *out_revents)
{
    if (custody->inode->type == InodeType::Directory)
        return true;

    return custody->inode->file_ops->poll(custody->inode, events, out_revents);
}

int vfs_mmap(FileCustody *custody, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags)
{
    if (custody->inode->type == InodeType::Directory)
        return -ERR_ISDIR;

    return custody->inode->file_ops->mmap(custody->inode, as, vaddr, length, flags);
}

int vfs_istty(FileCustody *custody)
{
    if (custody->inode->type != InodeType::CharacterDevice)
        return 0;

    return custody->inode->file_ops->istty(custody->inode);
}
