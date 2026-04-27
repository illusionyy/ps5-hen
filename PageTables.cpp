#include <stdint.h>
#include <sys/types.h>
#include <cstdio>
#include <optional>

#include "gpu_dma.h"
#include "util.h"
#include "PageTables.h"

#define print(fmt, ...) printf(fmt, ##__VA_ARGS__)

// Merge a clear-mask and set-mask into a PTE: first clear the bits in `clear`,
// then OR in the bits from `set`. Used for both guest and nested patches.
static inline PTE merge_bits(PTE entry, PTE clear, PTE set)
{
    return PTE{ .raw = (entry.raw & ~clear.raw) | set.raw };
}

// Extract the 9-bit index into a given paging level from a 48-bit virtual address.
// Levels: 4 = PML4, 3 = PDPT, 2 = PD, 1 = PT.
//
// x86-64 4-level paging splits a 48-bit virtual address like this:
//
//   47        39 38        30 29        21 20        12 11         0
//  +-----------+-------------+-------------+-------------+-----------+
//  | PML4 idx  |  PDPT idx   |   PD idx    |   PT idx    | page off  |
//  |  9 bits   |   9 bits    |   9 bits    |   9 bits    |  12 bits  |
//  +-----------+-------------+-------------+-------------+-----------+
//
// - The low 12 bits are the offset within a 4 KiB page (2^12 = 4096), so every
//   table index lives above bit 12.
// - Each paging level consumes 9 bits (a table has 512 = 2^9 entries, since a
//   4 KiB table holds 512 * 8-byte entries).
// - Level 1 (PT) starts at bit 12, level 2 (PD) at bit 21, level 3 (PDPT) at
//   bit 30, level 4 (PML4) at bit 39.
// - 0x1FF masks off the low 9 bits to isolate the index for that level.
static inline int level_index(uint64_t addr, int level)
{
    const int shift = 12 + 9 * (level - 1);
    return (addr >> shift) & 0x1FF;
}

struct PageTableCache
{
    using ReadFn  = int (*)(uint64_t phys_addr, void* buffer, size_t size);
    using WriteFn = int (*)(uint64_t phys_addr, const void* buffer, size_t size);

    ReadFn  read_phys  = nullptr;
    WriteFn write_phys = nullptr;

    // One cached table per non-root paging level. Indexed by level-1, so:
    //   levels[0] = PT   (level 1)
    //   levels[1] = PD   (level 2)
    //   levels[2] = PDPT (level 3)
    // PML4 (level 4) is supplied by the caller and not cached here.
    struct Level
    {
        uint64_t pa = 0;       // physical addr currently loaded (0 = empty)
        PTE      entries[512]{};
        bool     dirty = false;
    };

    static constexpr int kCachedLevels = 3;
    Level levels[kCachedLevels]{};

    PageTableCache(ReadFn r, WriteFn w) : read_phys(r), write_phys(w) {}

    // Flush a single level if dirty. `level` is 1..3 (PT, PD, PDPT).
    void flush_level(int level)
    {
        Level& L = levels[level - 1];
        if (!L.dirty) return;
        write_phys(L.pa, L.entries, sizeof(L.entries));
        L.dirty = false;
    }

    // Flush this level and every shallower (closer-to-leaf) level.
    // Required before evicting an upper-level cache, since a stale lower
    // level might belong to a different parent entry.
    void flush_from(int level)
    {
        for (int i = 1; i <= level; ++i) flush_level(i);
    }

    void flush() { flush_from(kCachedLevels); }

    // Load `pa` into the cache slot for `level` (1..3).
    // No-op if already cached. Evicts shallower levels when the parent
    // changes, since their contents are now potentially orphaned.
    bool load(int level, uint64_t pa)
    {
        Level& L = levels[level - 1];
        if (pa == L.pa && L.pa != 0) return true;

        flush_from(level);              // flush this + everything below
        L.pa = pa;
        return read_phys(pa, L.entries, sizeof(L.entries)) == 0;
    }

    static void patch_entry(PTE& entry, PTE clear, PTE set, bool& dirty)
    {
        PTE updated = merge_bits(entry, clear, set);
        if (updated.raw == entry.raw) return;
        entry = updated;
        dirty = true;
    }

    // Walk PML4 → PDPT → PD → PT for `addr` and patch the leaf. Returns the
    // pre-patch leaf, or nullopt if the walk terminates on a non-present
    // entry. Stops early at 1 GiB (PDPT) or 2 MiB (PD) leaves.
    std::optional<PTE> patch(const PTE* pml4, uint64_t addr, PTE clear, PTE set)
    {
        const PTE& pml4e = pml4[level_index(addr, 4)];
        if (!pml4e.present) return std::nullopt;

        // Walk levels 3 → 2 → 1, stopping at the first leaf.
        uint64_t next_pa = pml4e.addr();
        for (int level = 3; level >= 1; --level)
        {
            if (!load(level, next_pa)) return std::nullopt;

            Level& L = levels[level - 1];
            PTE& entry = L.entries[level_index(addr, level)];
            if (!entry.present) return std::nullopt;

            // Leaf: PT entry is always a leaf; PDPT/PD entries are leaves
            // only when page_size is set (1 GiB or 2 MiB pages).
            const bool is_leaf = (level == 1) || entry.page_size;
            if (is_leaf)
            {
                PTE leaf = entry;
                patch_entry(entry, clear, set, L.dirty);
                return leaf;
            }

            next_pa = entry.addr();
        }

        return std::nullopt; // unreachable: level==1 is always a leaf
    }
};

static int guest_read_adapter(uint64_t addr, void* buffer, size_t size)
{
    return kernel_copyout(get_dmap_addr(addr), buffer, size);
}

static int guest_write_adapter(uint64_t addr, const void* buffer, size_t size)
{
    return kernel_copyin(buffer, get_dmap_addr(addr), size);
}

static int nested_read_adapter(uint64_t addr, void* buffer, size_t size)
{
    return gpu_read_phys(addr, buffer, static_cast<uint32_t>(size));
}

static int nested_write_adapter(uint64_t addr, const void* buffer, size_t size)
{
    return gpu_write_phys(addr, buffer, static_cast<uint32_t>(size));
}

PageTables::PageTables(uint64_t guest_cr3, uint64_t nested_cr3)
    : m_gcr3(guest_cr3), m_ncr3(nested_cr3)
{
}

// For every 4 KB page in [va_start, va_end):
//   1. Walk the guest page tables to patch the leaf entry and resolve the GPA.
//   2. Walk the nested page tables to patch the host-side leaf for that GPA.
//
// Both walks use the same cached walker to minimize redundant DMA. Returns
// the number of pages for which the guest walk succeeded.
int PageTables::patch(uint64_t va_start, uint64_t va_end, PTE clear, PTE set)
{
    if (!m_gcr3 || !m_ncr3)
    {
        print("[pt] patch aborted — guest pmap or nested CR3 not set\n");
        return 0;
    }

    PTE guest_pml4[512]{};
    if (kernel_copyout(static_cast<intptr_t>(m_gcr3), guest_pml4, sizeof(guest_pml4)) != 0)
    {
        print("[pt] failed to read guest PML4 @ %lX\n", m_gcr3);
        return 0;
    }

    PTE nested_pml4[512]{};
    if (gpu_read_phys(m_ncr3, nested_pml4, sizeof(nested_pml4)) != 0)
    {
        print("[npt] failed to read nested PML4 @ %lX\n", m_ncr3);
        return 0;
    }

    PageTableCache guest_walker(&guest_read_adapter, &guest_write_adapter);
    PageTableCache nested_walker(&nested_read_adapter, &nested_write_adapter);

    int count = 0;
    for (uint64_t va = va_start; va < va_end; va += 0x1000)
    {
        // Guest walk: patch leaf and get the mapped GPA.
        auto guest_leaf = guest_walker.patch(guest_pml4, va, clear, set);
        if (!guest_leaf)
        {
            continue;
        }

        const uint64_t gpa = guest_leaf->addr();

        // Nested walk: patch the host-side leaf for that GPA.
        nested_walker.patch(nested_pml4, gpa, clear, set);
        count++;
    }

    guest_walker.flush();
    nested_walker.flush();
    return count;
}