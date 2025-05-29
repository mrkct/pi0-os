#include "fs.h"
#include "fat32/fat32.h"

#define LOG_ENABLED
#define LOG_TAG "FS"
#include <kernel/log.h>


Inode *icache_lookup(InodeCache *icache, InodeIdentifier identifier)
{
    auto *entry = icache->list.find_first([&](InodeCache::Entry *entry) {
        return entry->identifier == identifier;
    });
    if (entry == nullptr) {
        LOGD("Looking up inode %" PRIu64 " in icache ... failed", identifier);
        return nullptr;
    }

    LOGD("Looking up inode %" PRIu64 " in icache ... found (refcount: %d)", identifier, entry->inode->refcount);
    return entry->inode;
}

int icache_insert(InodeCache *icache, InodeIdentifier identifier, Inode *inode)
{
    kassert(icache_lookup(icache, identifier) == nullptr);

    auto *entry = (InodeCache::Entry*) malloc(sizeof(InodeCache::Entry));
    if (entry == nullptr)
        return -ERR_NOMEM;

    *entry = InodeCache::Entry {
        .prev = nullptr,
        .next = nullptr,
        .identifier = identifier,
        .inode = inode
    };
    icache->list.add(entry);

    LOGD("Inserting inode %" PRIu64 " into icache", identifier);
    return 0;
}

#if 0
static Inode *icache_remove(InodeCache *icache, InodeIdentifier identifier)
{
    auto *entry = icache->list.find_first([&](InodeCache::Entry *entry) {
        return entry->identifier == identifier;
    });
    if (entry == nullptr)
        return nullptr;
    
    icache->list.remove(entry);
    Inode *inode = entry->inode;
    free(entry);

    return inode;
}
#endif


uint64_t default_checked_seek(uint64_t filesize, uint64_t current, int whence, int32_t offset)
{
    // FIXME: This doesn't handle overflow very well
    uint64_t newoff;
    switch (whence) {
    case SEEK_SET:
        newoff = offset;
        break;
    case SEEK_CUR:
        newoff = current + offset;
        break;
    case SEEK_END:
        newoff = ((int64_t) filesize) - offset;
        break;
    }
    return clamp<uint64_t>(0, newoff, filesize);
}

Filesystem *fs_detect_and_create(BlockDevice &device)
{
    int rc;
    Filesystem *fs = nullptr;
    
    rc = fat32_try_create(device, &fs);
    if (rc == 0) {
        LOGI("Device '%s' is detected as FAT32", device.name());
        return fs;
    } else if (rc < 0 && rc != -ERR_INVAL) {
        LOGE("Device '%s' is detected as FAT32, but failed to mount: %d", device.name(), rc);
        return nullptr;
    }

    LOGE("Failed to find a compatible filesystem on device '%s'", device.name());
    return nullptr;
}

int32_t fs_inode_ioctl_not_supported(Inode*, uint32_t, void*)
{
    return -ERR_NOTSUP;
}

uint64_t fs_inode_seek_not_supported(Inode*, uint64_t, int, int32_t)
{
    return -ERR_NOTSUP;
}

int32_t fs_file_inode_poll_always_ready(Inode*, uint32_t events, uint32_t *out_revents)
{
    *out_revents = events & F_POLLMASK;
    return 0;
}

int32_t fs_file_inode_mmap_not_supported(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_create_not_supported(Inode*, const char*, InodeType, Inode *)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_mkdir_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_rmdir_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_unlink_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

int64_t fs_dir_inode_getdents_not_supported(Inode*, int64_t, struct dirent*, size_t)
{
    return -ERR_NOTSUP;
}

int32_t fs_file_inode_istty_always_false(Inode*)
{
    return 0;
}

const char* log_canonicalized_path(const char *path)
{
    static char buf[256];
    size_t i = 0;
    for (i = 0; path[i] != '\0' || path[i+1] != '\0'; i++) {
        buf[i] = path[i] == '\0' ? '/' : path[i];
    }
    buf[i] = '\0';
    return buf;
}

size_t canonicalized_path_strlen(const char *path)
{
    size_t len = 0;
    while (path[len] != '\0' || path[len+1] != '\0')
        len++;
    return len;
}

bool canonicalized_path_startswith(const char *path, const char *prefix)
{
    size_t pathlen = canonicalized_path_strlen(path);
    size_t prefixlen = canonicalized_path_strlen(prefix);
    if (pathlen < prefixlen)
        return false;
    return memcmp(path, prefix, prefixlen) == 0;
}

/**
 * Creates a canonicalized path from a string.
 * 
 * A canonicalized path is defined as a path where:
 * - There is no leading separator ('hello', not '/hello')
 * - There is no trailing separator ('hello', not 'hello/)
 * - There are no '.' or '..' components ('hello/world', not 'hello/./useless/../world')
 * 
 * The separators in the returned path are replaced with '\0'.
 * This makes it easier to iterate over the path components.
 * The end of the path is marked by an empty component
 * (meaning there are 2 '\0' at the end) 
 * 
 * This function allocates a new string and returns it.
 * It is your duty to free it.
 * 
*/
char *canonicalize_path(const char *path)
{
    while (*path == '/' && *(path + 1) == '/')
        path++;

    size_t pathlen = strlen(path);
    char *cpath = (char*) malloc(pathlen + 2);
    if (cpath == NULL)
        return NULL;

    memcpy(cpath, path, pathlen);
    cpath[pathlen] = '\0';
    cpath[pathlen + 1] = '\0';

    /** 
     * The code below does 3 things:
     *  1. First, it processes '..' by turning the previous component into all '/'.
     *     Example: 'hello/../world' becomes '////////world'.
     *  2. Then, it transforms all '/.' into '//'.
     *     Example: 'hello/./world.txt' becomes 'hello///world.txt'.
     *  3. Finally, it collapses consecutive separators into a single one.
     *     Example: 'hello///world' becomes 'hello/world'.
     * 
     * Last, it replaces all '/' with '\0', and adds an extra '\0' at the end
     * to mark the end of the path.
     */

    // Handle '..'
    {
        char *src = cpath;
        char *last_component_start = src;
        char *last_component_end = src;
        while (*src) {
            if (*src == '/') {
                last_component_start = last_component_end;
                last_component_end = src;
            }
            if (*src != '.' || *(src + 1) != '.') {
                src++;
                continue;
            }

            // We found a '..', so we need to remove the previous component
            memset(last_component_start, '/', (src + 1) - last_component_start);
            src += 2;
        }
    }

    // Handle '/.'
    {
        char *src = cpath;
        while (*src && *(src + 1)) {
            if (*src == '/' && *(src + 1) == '.') {
                *(src + 1) = '/';
            }
            src++;
        }
    }

    // Collapse consecutive separators ('hello///world' into 'hello/world')
    {
        char *src = cpath;
        char *cpath_end = cpath + pathlen + 2;

        while (*src) {
            if (*src != '/') {
                src++;
                continue;
            }

            // Count how many '/' there are
            int count = 0;
            char *temp = src;
            while (*temp == '/') {
                temp++;
                count++;
            }


            if (count == 1) {
                src++;
                continue;
            }

            kassert(count > 1);
            memmove(src + 1, temp, cpath_end - temp);
            src++;
        }
    }

    size_t cpath_len = strlen(cpath);
    for (size_t i = 0; i < cpath_len; i++) {
        if (cpath[i] == '/')
            cpath[i] = '\0';
    }
    cpath[cpath_len] = '\0';
    cpath[cpath_len + 1] = '\0';

    return cpath;
}

/**
 * Basically adds back the '/' to a canonicalized path
 */
void decanonicalize_path(char *path)
{
    /* This is the special case of '/', which becomes 3 \0 in sequence */
    if (*path == '\0' && *(path + 1) == '\0') {
        *path = '/';
        return;
    }

    char *src = path;
    while (*src != '\0' || *(src + 1) != '\0') {
        if (*src == '\0')
            *src = '/';
        src++;
    }
}

char *pathjoin(const char *path1, const char *path2)
{
    size_t len1 = strnlen(path1, MAX_PATH_LEN);
    size_t len2 = strnlen(path2, MAX_PATH_LEN);
    if (len1 + len2 + 2 > MAX_PATH_LEN) {
        LOGE("Path too long: '%s' + '%s'", path1, path2);
        return nullptr;
    }

    char *result = (char*) malloc(len1 + len2 + 2);
    if (result == nullptr) {
        LOGE("Failed to allocate memory for pathjoin");
        return nullptr;
    }

    memcpy(result, path1, len1);
    if (len1 > 0 && result[len1 - 1] != '/') {
        result[len1] = '/';
        len1++;
    }
    memcpy(result + len1, path2, len2);
    result[len1 + len2] = '\0';

    return result;
}
