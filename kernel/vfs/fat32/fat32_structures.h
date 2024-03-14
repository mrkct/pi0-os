#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::fat32 {

static constexpr uint16_t BOOT_SIGNATURE = 0xaa55;

struct BiosParameterBlock {
    uint8_t BS_JmpBoot[3];
    uint8_t BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t BPB_Reserved[12];
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    uint8_t BS_VolLab[11];
    uint8_t BS_FilSysType[8];
    uint8_t BS_BootCode32[420];
    uint16_t BS_BootSig32;
} __attribute__((packed));

static constexpr uint32_t FSINFO_LEAD_SIGNATURE = 0x41615252;
static constexpr uint32_t FSINFO_STRUC_SIGNATURE = 0x61417272;
static constexpr uint32_t FSINFO_TRAIL_SIGNATURE = 0xaa550000;

struct FSInfo {
    uint32_t FSI_LeadSig;
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__((packed));
static_assert(sizeof(FSInfo) == 512, "FSInfo size must be equal to sector size");

struct DirectoryEntry8_3 {
    static constexpr uint8_t ATTR_READ_ONLY = 0x01;
    static constexpr uint8_t ATTR_HIDDEN = 0x02;
    static constexpr uint8_t ATTR_SYSTEM = 0x04;
    static constexpr uint8_t ATTR_VOLUME_ID = 0x08;
    static constexpr uint8_t ATTR_DIRECTORY = 0x10;
    static constexpr uint8_t ATTR_ARCHIVE = 0x20;
    static constexpr uint8_t ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;

    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;

    bool is_end_of_directory() const { return DIR_Name[0] == 0; }
    bool is_cancelled() const { return DIR_Name[0] == 0xe5; }
} __attribute__((packed));
static_assert(sizeof(DirectoryEntry8_3) == 32, "DirectoryEntry8_3 size must be 32 bytes");

union DirectoryEntry {
    uint8_t raw[32];
    DirectoryEntry8_3 entry;
};
static_assert(sizeof(DirectoryEntry) == 32, "DirectoryEntry size must be 32 bytes");

static inline bool is_valid_cluster(uint32_t cluster)
{
    return cluster < 0x0ffffff8;
}

}
