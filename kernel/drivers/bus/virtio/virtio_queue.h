#ifndef VIRTQUEUE_H
#define VIRTQUEUE_H

#include <stdint.h>

typedef uint64_t le64;
typedef uint32_t le32;
typedef uint32_t u32;
typedef uint16_t le16;
typedef uint8_t  u8;

/* An interface for efficient virtio implementation.
 *
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers.
 *
 * Copyright 2007, 2009, IBM Corporation
 * Copyright 2011, Red Hat, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdint.h>

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT   4

/* The device uses this in used->flags to advise the driver: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization. */
#define VIRTQ_USED_F_NO_NOTIFY  1
/* The driver uses this in avail->flags to advise the device: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VIRTQ_AVAIL_F_NO_INTERRUPT      1

/* Support for indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC    28

/* Support for avail_event and used_event fields */
#define VIRTIO_F_EVENT_IDX        29

/* Arbitrary descriptor layouts. */
#define VIRTIO_F_ANY_LAYOUT       27

/* Virtqueue descriptors: 16 bytes.
 * These can chain together via "next". */
struct virtq_desc {
        /* Address (guest-physical). */
        le64 addr;
        /* Length. */
        le32 len;
        /* The flags as indicated above. */
        le16 flags;
        /* We chain unused descriptors via this, too */
        le16 next;
};

struct virtq_avail {
        le16 flags;
        le16 idx;
        le16 ring[];
        /* Only if VIRTIO_F_EVENT_IDX: le16 used_event; */
};

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
        /* Index of start of used descriptor chain. */
        le32 id;
        /* Total length of the descriptor chain which was written to. */
        le32 len;
};

struct virtq_used {
        le16 flags;
        le16 idx;
        struct virtq_used_elem ring[];
        /* Only if VIRTIO_F_EVENT_IDX: le16 avail_event; */
};

struct virtq {
        unsigned int num;

        struct virtq_desc *desc;
        struct virtq_avail *avail;
        struct virtq_used *used;
};

static inline int virtq_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old_idx)
{
         return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old_idx);
}

/* Get location of event indices (only with VIRTIO_F_EVENT_IDX) */
static inline le16 *virtq_used_event(struct virtq *vq)
{
        /* For backwards compat, used event index is at *end* of avail ring. */
        return &vq->avail->ring[vq->num];
}

static inline le16 *virtq_avail_event(struct virtq *vq)
{
        /* For backwards compat, avail event index is at *end* of used ring. */
        return (le16 *)&vq->used->ring[vq->num];
}

/* Block device (adapted) */

struct virtio_blk_req_header {
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13
        le32 type;
        le32 reserved;
        le64 sector;
} __attribute__((packed));
static_assert(sizeof(struct virtio_blk_req_header) == 16);

struct virtio_blk_req_footer {
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2
        u8 status;
} __attribute__((packed));
static_assert(sizeof(struct virtio_blk_req_footer) == 1);

/* GPU device */

#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0) 
 
struct virtio_gpu_config { 
        le32 events_read; 
        le32 events_clear; 
        le32 num_scanouts; 
        le32 num_capsets; 
}; 

enum virtio_gpu_shm_id { 
        VIRTIO_GPU_SHM_ID_UNDEFINED = 0, 
        VIRTIO_GPU_SHM_ID_HOST_VISIBLE = 1, 
}; 

enum virtio_gpu_ctrl_type { 
 
        /* 2d commands */ 
        VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100, 
        VIRTIO_GPU_CMD_RESOURCE_CREATE_2D, 
        VIRTIO_GPU_CMD_RESOURCE_UNREF, 
        VIRTIO_GPU_CMD_SET_SCANOUT, 
        VIRTIO_GPU_CMD_RESOURCE_FLUSH, 
        VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D, 
        VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING, 
        VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING, 
        VIRTIO_GPU_CMD_GET_CAPSET_INFO, 
        VIRTIO_GPU_CMD_GET_CAPSET, 
        VIRTIO_GPU_CMD_GET_EDID, 
        VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID, 
        VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB, 
        VIRTIO_GPU_CMD_SET_SCANOUT_BLOB, 
 
        /* 3d commands */ 
        VIRTIO_GPU_CMD_CTX_CREATE = 0x0200, 
        VIRTIO_GPU_CMD_CTX_DESTROY, 
        VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, 
        VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE, 
        VIRTIO_GPU_CMD_RESOURCE_CREATE_3D, 
        VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D, 
        VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D, 
        VIRTIO_GPU_CMD_SUBMIT_3D, 
        VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB, 
        VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB, 
 
        /* cursor commands */ 
        VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300, 
        VIRTIO_GPU_CMD_MOVE_CURSOR, 
 
        /* success responses */ 
        VIRTIO_GPU_RESP_OK_NODATA = 0x1100, 
        VIRTIO_GPU_RESP_OK_DISPLAY_INFO, 
        VIRTIO_GPU_RESP_OK_CAPSET_INFO, 
        VIRTIO_GPU_RESP_OK_CAPSET, 
        VIRTIO_GPU_RESP_OK_EDID, 
        VIRTIO_GPU_RESP_OK_RESOURCE_UUID, 
        VIRTIO_GPU_RESP_OK_MAP_INFO, 
 
        /* error responses */ 
        VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200, 
        VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY, 
        VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID, 
        VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID, 
        VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID, 
        VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER, 
}; 
 
#define VIRTIO_GPU_FLAG_FENCE (1 << 0) 
#define VIRTIO_GPU_FLAG_INFO_RING_IDX (1 << 1) 
 
struct virtio_gpu_ctrl_hdr { 
        le32 type; 
        le32 flags; 
        le64 fence_id; 
        le32 ctx_id; 
        u8 ring_idx; 
        u8 padding[3]; 
};

#define VIRTIO_GPU_MAX_SCANOUTS 16 
 
struct virtio_gpu_rect { 
        le32 x; 
        le32 y; 
        le32 width; 
        le32 height; 
}; 
 
struct virtio_gpu_resp_display_info { 
        struct virtio_gpu_ctrl_hdr hdr; 
        struct virtio_gpu_display_one { 
                struct virtio_gpu_rect r; 
                le32 enabled; 
                le32 flags; 
        } pmodes[VIRTIO_GPU_MAX_SCANOUTS]; 
};

struct virtio_gpu_get_edid { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 scanout; 
        le32 padding; 
}; 
 
struct virtio_gpu_resp_edid { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 size; 
        le32 padding; 
        u8 edid[1024]; 
};

enum virtio_gpu_formats { 
        VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1, 
        VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2, 
        VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3, 
        VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4, 
 
        VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67, 
        VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68, 
 
        VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121, 
        VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134, 
}; 
 
struct virtio_gpu_resource_create_2d { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 format; 
        le32 width; 
        le32 height; 
};

struct virtio_gpu_set_scanout { 
        struct virtio_gpu_ctrl_hdr hdr; 
        struct virtio_gpu_rect r; 
        le32 scanout_id; 
        le32 resource_id; 
};

struct virtio_gpu_resource_flush { 
        struct virtio_gpu_ctrl_hdr hdr; 
        struct virtio_gpu_rect r; 
        le32 resource_id; 
        le32 padding; 
};

struct virtio_gpu_transfer_to_host_2d { 
        struct virtio_gpu_ctrl_hdr hdr; 
        struct virtio_gpu_rect r; 
        le64 offset; 
        le32 resource_id; 
        le32 padding; 
};

struct virtio_gpu_resource_attach_backing { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 nr_entries; 
}; 
 
struct virtio_gpu_mem_entry { 
        le64 addr; 
        le32 length; 
        le32 padding; 
};

struct virtio_gpu_resource_detach_backing { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 padding; 
};

struct virtio_gpu_get_capset_info { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 capset_index; 
        le32 padding; 
}; 
 
#define VIRTIO_GPU_CAPSET_VIRGL 1 
#define VIRTIO_GPU_CAPSET_VIRGL2 2 
#define VIRTIO_GPU_CAPSET_GFXSTREAM 3 
#define VIRTIO_GPU_CAPSET_VENUS 4 
#define VIRTIO_GPU_CAPSET_CROSS_DOMAIN 5 
struct virtio_gpu_resp_capset_info { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 capset_id; 
        le32 capset_max_version; 
        le32 capset_max_size; 
        le32 padding; 
};

struct virtio_gpu_get_capset { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 capset_id; 
        le32 capset_version; 
}; 
 
struct virtio_gpu_resp_capset { 
        struct virtio_gpu_ctrl_hdr hdr; 
        u8 capset_data[]; 
};

struct virtio_gpu_resource_assign_uuid { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 padding; 
}; 
 
struct virtio_gpu_resp_resource_uuid { 
        struct virtio_gpu_ctrl_hdr hdr; 
        u8 uuid[16]; 
};

#define VIRTIO_GPU_BLOB_MEM_GUEST             0x0001 
#define VIRTIO_GPU_BLOB_MEM_HOST3D            0x0002 
#define VIRTIO_GPU_BLOB_MEM_HOST3D_GUEST      0x0003 
 
#define VIRTIO_GPU_BLOB_FLAG_USE_MAPPABLE     0x0001 
#define VIRTIO_GPU_BLOB_FLAG_USE_SHAREABLE    0x0002 
#define VIRTIO_GPU_BLOB_FLAG_USE_CROSS_DEVICE 0x0004 
 
struct virtio_gpu_resource_create_blob { 
       struct virtio_gpu_ctrl_hdr hdr; 
       le32 resource_id; 
       le32 blob_mem; 
       le32 blob_flags; 
       le32 nr_entries; 
       le64 blob_id; 
       le64 size; 
};

struct virtio_gpu_set_scanout_blob { 
       struct virtio_gpu_ctrl_hdr hdr; 
       struct virtio_gpu_rect r; 
       le32 scanout_id; 
       le32 resource_id; 
       le32 width; 
       le32 height; 
       le32 format; 
       le32 padding; 
       le32 strides[4]; 
       le32 offsets[4]; 
};

#define VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK 0x000000ff; 
struct virtio_gpu_ctx_create { 
       struct virtio_gpu_ctrl_hdr hdr; 
       le32 nlen; 
       le32 context_init; 
       char debug_name[64]; 
};

struct virtio_gpu_resource_map_blob { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 padding; 
        le64 offset; 
}; 
 
#define VIRTIO_GPU_MAP_CACHE_MASK      0x0f 
#define VIRTIO_GPU_MAP_CACHE_NONE      0x00 
#define VIRTIO_GPU_MAP_CACHE_CACHED    0x01 
#define VIRTIO_GPU_MAP_CACHE_UNCACHED  0x02 
#define VIRTIO_GPU_MAP_CACHE_WC        0x03 
struct virtio_gpu_resp_map_info { 
        struct virtio_gpu_ctrl_hdr hdr; 
        u32 map_info; 
        u32 padding; 
};

struct virtio_gpu_resource_unmap_blob { 
        struct virtio_gpu_ctrl_hdr hdr; 
        le32 resource_id; 
        le32 padding; 
};

struct virtio_gpu_cursor_pos { 
        le32 scanout_id; 
        le32 x; 
        le32 y; 
        le32 padding; 
}; 
 
struct virtio_gpu_update_cursor { 
        struct virtio_gpu_ctrl_hdr hdr; 
        struct virtio_gpu_cursor_pos pos; 
        le32 resource_id; 
        le32 hot_x; 
        le32 hot_y; 
        le32 padding; 
};

/* Input device */

enum virtio_input_config_select {
        VIRTIO_INPUT_CFG_UNSET = 0x00,
        VIRTIO_INPUT_CFG_ID_NAME = 0x01,
        VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
        VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
        VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
        VIRTIO_INPUT_CFG_EV_BITS = 0x11,
        VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
};

struct virtio_input_absinfo {
        le32 min;
        le32 max;
        le32 fuzz;
        le32 flat;
        le32 res;
};

struct virtio_input_devids {
        le16 bustype;
        le16 vendor;
        le16 product;
        le16 version;
};

struct virtio_input_config {
        u8 select;
        u8 subsel;
        u8 size;
        u8 reserved[5];
        union {
                char string[128];
                u8 bitmap[128];
                struct virtio_input_absinfo abs;
                struct virtio_input_devids ids;
        } u;
};

struct virtio_input_event {
        le16 type;
        le16 code;
        le32 value;
};

#endif /* VIRTQUEUE_H */
