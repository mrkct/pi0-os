#pragma once

#include <kernel/base.h>
#include "fs.h"


struct FileCustody {
    Inode *inode;
    uint32_t flags;
    uint64_t offset;
};

int vfs_mount(const char *path, Filesystem&);

int vfs_open(const char *path, uint32_t flags, FileCustody** );

ssize_t vfs_read(FileCustody*, uint8_t *buffer, uint32_t size);

ssize_t vfs_write(FileCustody*, uint8_t const *buffer, uint32_t size);

ssize_t vfs_seek(FileCustody*, int whence, int32_t);

int vfs_close(FileCustody*);

int vfs_ioctl(FileCustody*, uint32_t ioctl, void *argp);

int vfs_stat(const char *path, api::Stat*);

int vfs_fstat(FileCustody *custody, api::Stat *stat);

FileCustody* vfs_duplicate(FileCustody*);

int vfs_create_pipe(FileCustody **out_sender_custody, FileCustody **out_receiver_custody);

int32_t vfs_poll(FileCustody *custody, uint32_t events, uint32_t *out_revents);

int vfs_mmap(FileCustody *custody, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags);
