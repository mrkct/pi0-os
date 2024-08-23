#include "armv6mmu.h"


const char *dfsr_status_to_string(uint32_t dfsr_status)
{
    switch (dfsr_status) {
    case 0b00001: return "Alignment";
    case 0b00000:
    case 0b00011: return "PMSA - TLB miss";
    case 0b00100: return "Instruction Cache Maintenance Operation Fault";
    case 0b01100: return "External Abort on Translation (1st Level)";
    case 0b01110: return "External Abort on Translation (2nd level)";
    case 0b00101: return "Translation (Section)";
    case 0b00111: return "Translation (Page)";
    case 0b01001: return "Domain (Section)";
    case 0b01011: return "Domain (Page)";
    case 0b01101: return "Permission (Section)";
    case 0b01111: return "Permission (Page)";
    case 0b01000: return "Precise External Abort";
    case 0b01010: return "External Abort, Precise";
    case 0b10100: return "TLB Lock";
    case 0b11010: return "Coprocessor Data Abort";
    case 0b10110: return "Imprecise External Abort";
    case 0b11000: return "Parity Error Exception";
    case 0b00010: return "Debug Event";
    default: return "Unknown";
    }
}
