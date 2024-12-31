#include "elfloader.h"
#include "elf.h"
#include <kernel/vfs/vfs.h>

#define LOG_ENABLED
#define LOG_TAG "ELF"
#include <kernel/log.h>


static int verify_identification(Elf32_Ehdr const *header)
{
    if (header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        LOGW("Invalid magic number");
        return -ERR_NOEXEC;
    }
    
    if (header->e_ident[4] != ELFCLASS32 ||
        header->e_ident[5] != ELFDATA2LSB ||
        header->e_ident[6] != EV_CURRENT) {
        
        LOGW("Unsupported ELF type");
        return -ERR_NOTSUP;
    }

    return 0;
}

static int try_load_elf(uint8_t const *elf_binary, size_t binary_size, AddressSpace& as, uintptr_t &entry_point)
{
    int rc = 0;
    Elf32_Ehdr const* header;

    if (binary_size < sizeof(Elf32_Ehdr)) {
        LOGE("Binary is smaller than the ELF header");
        rc = -ERR_NOEXEC;
        goto cleanup;
    }
    
    header = reinterpret_cast<Elf32_Ehdr const*>(elf_binary);
    rc = verify_identification(header);
    if (rc != 0) {
        LOGE("Binary didn't pass the identification process");
        goto cleanup;
    }

    if (header->e_type != ET_EXEC) {
        LOGE("ELF is not executable");
        rc = -ERR_NOEXEC;
        goto cleanup;
    } else if (header->e_machine != EM_ARM) {
        LOGE("ELF is for incompatible architecture"); 
        rc = -ERR_NOTSUP;
        goto cleanup;
    }

    entry_point = header->e_entry;

    rc = vm_using_address_space(as, [&]() {
        Error error;
        auto const *program_header = reinterpret_cast<Elf32_Phdr const*>(elf_binary + header->e_phoff);
        for (size_t i = 0; i < header->e_phnum; i++) {
            auto const *p_hdr = &program_header[i];

            if (p_hdr->p_type != PT_LOAD)
                continue;
            
            auto start = round_down<uintptr_t>(p_hdr->p_vaddr, 4 * _1KB);
            auto end = round_up<uintptr_t>(p_hdr->p_paddr + p_hdr->p_memsz, 4 * _1KB);         
            for (uintptr_t addr = start; addr < end; addr += 4 * _1KB) {
                struct PhysicalPage* page;
                error = physical_page_alloc(PageOrder::_4KB, page);
                if (!error.is_success()) {
                    LOGE("Failed to alloc physical page");
                    return -ERR_NOMEM;
                }

                error = vm_map(as, page, addr, PageAccessPermissions::UserFullAccess);
                if (!error.is_success()) {
                    LOGE("Failed to map page %p at virt_addr %p", page2addr(page), addr);
                    physical_page_free(page, PageOrder::_4KB);
                    return -ERR_NOMEM;
                }

                vm_memset(as, addr, 0, 4 * _1KB);
            }

            if (p_hdr->p_filesz != 0)
                vm_copy_to_user(as, p_hdr->p_vaddr, elf_binary + p_hdr->p_offset, p_hdr->p_filesz);
        }

        return 0;
    });
    if (rc != 0)
        goto cleanup;

    return rc;    
cleanup:
    return rc;
}

int elf_load_into_address_space(const char *path, uintptr_t *entrypoint, AddressSpace &as)
{
    int rc;
    ssize_t fsize, read;
    FileCustody *custody = nullptr;
    uint8_t *binary = nullptr;
    
    rc = vfs_open(path, OF_RDONLY, &custody);
    if (rc != 0)
        goto cleanup;

    fsize = vfs_seek(custody, SEEK_END, 0);
    vfs_seek(custody, SEEK_SET, 0);

    binary = (uint8_t*) malloc(fsize);
    if (binary == nullptr) {
        rc = -ERR_NOMEM;
        goto cleanup;
    }

    LOGD("ELF binary allocated area: %p", binary);
    read = vfs_read(custody, binary, fsize);
    if (read < 0) {
        rc = (int) read;
        goto cleanup;
    }

    rc = try_load_elf(binary, fsize, as, *entrypoint);

cleanup:
    free(binary);
    vfs_close(custody);

    return rc;
}
