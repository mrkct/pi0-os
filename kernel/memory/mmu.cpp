#include <kernel/memory/areas.h>
#include <kernel/memory/mmu.h>

namespace kernel {

uintptr_t virt2phys(uintptr_t virt)
{
    if (areas::higher_half.contains(virt))
        return virt - 0xe0000000;
    else if (areas::peripherals.contains(virt))
        return virt;
    panic("virt2phys: address %p is not in the higher half or peripherals area, cannot convert it (yet)", virt);
}

}
