#pragma once
#include <stdint.h>

// x86-64 4-level page-table entry.
// Layout is identical for guest pmap entries and AMD NPT entries, which is
// why both paths share this union.
union PTE
{
    uint64_t raw;
    struct
    {
        uint64_t present       : 1;   // bit 0  — entry is valid; if 0, the
                                      //           rest of the bits are
                                      //           OS-defined (swap slot, etc.)
                                      //
        uint64_t write         : 1;   // bit 1  — writable. ANDed across all
                                      //           levels. Bypassable in ring 0
                                      //           when CR0.WP = 0.
                                      //
        uint64_t user          : 1;   // bit 2  — ring 3 may access. ANDed
                                      //           across levels. Interacts
                                      //           with SMEP / SMAP.
                                      //
        uint64_t write_through : 1;   // bit 3  — PWT, low bit of PAT index
                                      //
        uint64_t cache_disable : 1;   // bit 4  — PCD, middle bit of PAT index
                                      //
        uint64_t accessed      : 1;   // bit 5  — set by CPU on any access
                                      //           through this entry; set at
                                      //           every level walked. OS
                                      //           clears it for LRU aging.
                                      //
        uint64_t dirty         : 1;   // bit 6  — set by CPU on write through
                                      //           a leaf entry. Ignored on
                                      //           non-leaf entries.
                                      //
        uint64_t page_size     : 1;   // bit 7  — PS bit.
                                      //           PDPTE: 1 = maps 1 GB page
                                      //           PDE:   1 = maps 2 MB page
                                      //           PTE (4 KB leaf): repurposed
                                      //                  as the PAT bit (high
                                      //                  bit of PAT index).
                                      //           PML4E: reserved, must be 0.
                                      //
        uint64_t global        : 1;   // bit 8  — leaf-only, requires
                                      //           CR4.PGE = 1. TLB entry
                                      //           survives CR3 reload. KPTI
                                      //           strips this from user-
                                      //           visible tables.
                                      //
        uint64_t _avl          : 3;   // bits 9-11 — ignored by hardware; OS
                                      //              or hypervisor scratch
                                      //              (Windows uses these for
                                      //              prototype/transition
                                      //              PTE markers, etc.).
                                      //
        uint64_t pfn           : 40;  // bits 12-51 — physical frame number.
                                      //   Leaf:    physical page being mapped
                                      //   Non-leaf: physical addr of next
                                      //             level table.
                                      //   For 2 MB pages low 9 bits must be 0;
                                      //   for 1 GB pages low 18 bits must be 0
                                      //   (natural alignment requirement).
                                      //
        uint64_t _res          : 6;   // bits 52-57 — reserved on stock x86-64;
                                      //              must be zero or a
                                      //              reserved-bit #PF is
                                      //              raised. Bits above
                                      //              MAXPHYADDR are
                                      //              effectively free in
                                      //              practice but treated as
                                      //              reserved for portability.
                                      //
        uint64_t xotext        : 1;   // bit 58 — non-standard. Sony/AMD
                                      //           execute-only text extension:
                                      //           page is fetchable as code
                                      //           but not readable as data
                                      //           (anti-dump / anti-ROP).
                                      //           Reserved on stock CPUs.
                                      //
        uint64_t pkey          : 4;   // bits 59-62 — MPK / PKU / PKS index
                                      //              into PKRU (user) or
                                      //              PKRS (supervisor).
                                      //              Requires CR4.PKE / PKS.
                                      //              Lets software toggle R/W
                                      //              permissions per key
                                      //              without TLB shootdowns.
                                      //
        uint64_t nx            : 1;   // bit 63 — No-Execute
                                      //           Instruction fetches from
                                      //           this page raise #PF with
                                      //           the I/D fault bit set.
                                      //           ORed across all paging
                                      //           levels. Requires EFER.NXE.
    };

    // Convert the PFN field to a byte address (page-aligned, low 12 bits zero).
    // For non-leaf entries this points at the next page-table level; for leaf
    // entries it points at the start of the mapped page (4 KB, 2 MB, or 1 GB).
    uint64_t addr() const
    {
        return static_cast<uint64_t>(pfn) << 12;
    }
};
static_assert(sizeof(PTE) == 8, "PTE size mismatch");

// Patches the guest kernel pmap (via kernel_copyout/copyin) and the AMD NPT
// (via gpu_read/write_phys) together, keeping both tables in sync.
//
// Both walks use a shared cached walker implementation so contiguous ranges
// only reload each level's table when the walk crosses that level's boundary
// (every 2 MB for PT, 1 GB for PD, 512 GB for PDPT).
class PageTables
{
public:
    PageTables(uint64_t guest_cr3, uint64_t nested_cr3);

    // For every 4 KB page in [va_start, va_end):
    //   1. Walk the guest page tables, patch the leaf entry (PDPTE / PDE /
    //      PTE depending on page size), and resolve the mapped GPA.
    //   2. Walk the nested page tables from nested_cr3 down to the leaf
    //      covering that GPA and apply the same clear/set mask.
    //
    // Both walks share a cached walker so contiguous ranges minimize DMA.
    // Returns the number of pages for which the guest walk succeeded, or 0
    // if either CR3 is missing or a PML4 read fails.
    int patch(uint64_t va_start, uint64_t va_end, PTE clear, PTE set);

private:
    uint64_t m_gcr3;
    uint64_t m_ncr3;
};