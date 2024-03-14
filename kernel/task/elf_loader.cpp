#include <kernel/task/elf_loader.h>
#include <elf.h>
#include <kernel/lib/math.h>


namespace kernel {

static Error verify_identification(Elf32_Ehdr const *header)
{
    if (header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F')
        return BadParameters;
    
    if (header->e_ident[4] != ELFCLASS32 ||
        header->e_ident[5] != ELFDATA2LSB ||
        header->e_ident[6] != EV_CURRENT)
        return NotSupported;

    return Success;
}

Error try_load_elf(uint8_t const *elf_binary, size_t binary_size, AddressSpace& as, uintptr_t &entry_point, bool is_kernel_task)
{
    if (binary_size < sizeof(Elf32_Ehdr))
        return BadParameters;
    
    auto const *header = reinterpret_cast<Elf32_Ehdr const*>(elf_binary);
    TRY(verify_identification(header));

    if (header->e_type != ET_EXEC)
        return BadParameters;
    
    if (header->e_machine != EM_ARM)
        return NotSupported;

    entry_point = header->e_entry;

    return vm_using_address_space(as, [&]() {
        auto const *program_header = reinterpret_cast<Elf32_Phdr const*>(elf_binary + header->e_phoff);
        for (size_t i = 0; i < header->e_phnum; i++) {
            auto const *p_hdr = &program_header[i];

            if (p_hdr->p_type != PT_LOAD)
                continue;
            
            auto start = round_down<uintptr_t>(p_hdr->p_vaddr, 4 * _1KB);
            auto end = round_up<uintptr_t>(p_hdr->p_paddr + p_hdr->p_memsz, 4 * _1KB);         
            for (uintptr_t addr = start; addr < end; addr += 4 * _1KB) {
                struct PhysicalPage* page;
                // FIXME: Rollback if this fails
                MUST(physical_page_alloc(PageOrder::_4KB, page));
                // TODO: Would be cool to change permissions to read-only if the header says so
                MUST(vm_map(as, page, addr, 
                    is_kernel_task ? PageAccessPermissions::PriviledgedOnly : PageAccessPermissions::UserFullAccess
                ));

                vm_memset(as, addr, 0, 4 * _1KB);
            }

            if (p_hdr->p_filesz != 0)
                vm_copy_to_user(as, p_hdr->p_vaddr, elf_binary + p_hdr->p_offset, p_hdr->p_filesz);
        }

        return Success;
    });
}

}
