#include "qemu/osdep.h"
#include "qemu-common.h"

#include "sysemu/hvf.h"
#include "hvf-i386.h"
#include "hvf-utils/vmcs.h"
#include "hvf-utils/vmx.h"
#include "hvf-utils/x86.h"
#include "hvf-utils/x86_descr.h"
#include "hvf-utils/x86_mmu.h"
#include "hvf-utils/x86_decode.h"
#include "hvf-utils/x86_emu.h"
#include "hvf-utils/x86_cpuid.h"
#include "hvf-utils/x86hvf.h"

#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>

#include "exec/address-spaces.h"
#include "exec/exec-all.h"
#include "exec/ioport.h"
#include "hw/i386/apic_internal.h"
#include "hw/boards.h"
#include "qemu/main-loop.h"
#include "strings.h"
#include "trace.h"
#include "sysemu/accel.h"
#include "sysemu/sysemu.h"
#include "target/i386/cpu.h"

pthread_rwlock_t mem_lock = PTHREAD_RWLOCK_INITIALIZER;
HVFState *hvf_state;
static int hvf_disabled = 1;

static void assert_hvf_ok(hv_return_t ret)
{
    if (ret == HV_SUCCESS) {
        return;
    }

    switch (ret) {
    case HV_ERROR:
        fprintf(stderr, "Error: HV_ERROR\n");
        break;
    case HV_BUSY:
        fprintf(stderr, "Error: HV_BUSY\n");
        break;
    case HV_BAD_ARGUMENT:
        fprintf(stderr, "Error: HV_BAD_ARGUMENT\n");
        break;
    case HV_NO_RESOURCES:
        fprintf(stderr, "Error: HV_NO_RESOURCES\n");
        break;
    case HV_NO_DEVICE:
        fprintf(stderr, "Error: HV_NO_DEVICE\n");
        break;
    case HV_UNSUPPORTED:
        fprintf(stderr, "Error: HV_UNSUPPORTED\n");
        break;
    default:
        fprintf(stderr, "Unknown Error\n");
    }

    abort();
}

/* Memory slots */
hvf_slot *hvf_find_overlap_slot(uint64_t start, uint64_t end)
{
    hvf_slot *slot;
    int x;
    for (x = 0; x < hvf_state->num_slots; ++x) {
        slot = &hvf_state->slots[x];
        if (slot->size && start < (slot->start + slot->size) &&
            end > slot->start) {
            return slot;
        }
    }
    return NULL;
}

struct mac_slot {
    int present;
    uint64_t size;
    uint64_t gpa_start;
    uint64_t gva;
};

struct mac_slot mac_slots[32];
#define ALIGN(x, y)  (((x) + (y) - 1) & ~((y) - 1))

static int do_hvf_set_memory(hvf_slot *slot)
{
    struct mac_slot *macslot;
    hv_memory_flags_t flags;
    hv_return_t ret;

    macslot = &mac_slots[slot->slot_id];

    if (macslot->present) {
        if (macslot->size != slot->size) {
            macslot->present = 0;
            ret = hv_vm_unmap(macslot->gpa_start, macslot->size);
            assert_hvf_ok(ret);
        }
    }

    if (!slot->size) {
        return 0;
    }

    flags = HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC;

    macslot->present = 1;
    macslot->gpa_start = slot->start;
    macslot->size = slot->size;
    ret = hv_vm_map((hv_uvaddr_t)slot->mem, slot->start, slot->size, flags);
    assert_hvf_ok(ret);
    return 0;
}

void hvf_set_phys_mem(MemoryRegionSection *section, bool add)
{
    hvf_slot *mem;
    MemoryRegion *area = section->mr;

    if (!memory_region_is_ram(area)) {
        return;
    }

    mem = hvf_find_overlap_slot(
            section->offset_within_address_space,
            section->offset_within_address_space + int128_get64(section->size));

    if (mem && add) {
        if (mem->size == int128_get64(section->size) &&
            mem->start == section->offset_within_address_space &&
            mem->mem == (memory_region_get_ram_ptr(area) +
            section->offset_within_region)) {
            return; /* Same region was attempted to register, go away. */
        }
    }

    /* Region needs to be reset. set the size to 0 and remap it. */
    if (mem) {
        mem->size = 0;
        if (do_hvf_set_memory(mem)) {
            fprintf(stderr, "Failed to reset overlapping slot\n");
            abort();
        }
    }

    if (!add) {
        return;
    }

    /* Now make a new slot. */
    int x;

    for (x = 0; x < hvf_state->num_slots; ++x) {
        mem = &hvf_state->slots[x];
        if (!mem->size) {
            break;
        }
    }

    if (x == hvf_state->num_slots) {
        fprintf(stderr, "No free slots\n");
        abort();
    }

    mem->size = int128_get64(section->size);
    mem->mem = memory_region_get_ram_ptr(area) + section->offset_within_region;
    mem->start = section->offset_within_address_space;
    mem->region = area;

    if (do_hvf_set_memory(mem)) {
        fprintf(stderr, "Error registering new memory slot\n");
        abort();
    }
}

void vmx_update_tpr(CPUState *cpu)
{
    /* TODO: need integrate APIC handling */
    X86CPU *x86_cpu = X86_CPU(cpu);
    int tpr = cpu_get_apic_tpr(x86_cpu->apic_state) << 4;
    int irr = apic_get_highest_priority_irr(x86_cpu->apic_state);

    wreg(cpu->hvf_fd, HV_X86_TPR, tpr);
    if (irr == -1) {
        wvmcs(cpu->hvf_fd, VMCS_TPR_THRESHOLD, 0);
    } else {
        wvmcs(cpu->hvf_fd, VMCS_TPR_THRESHOLD, (irr > tpr) ? tpr >> 4 :
              irr >> 4);
    }
}

void update_apic_tpr(CPUState *cpu)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    int tpr = rreg(cpu->hvf_fd, HV_X86_TPR) >> 4;
    cpu_set_apic_tpr(x86_cpu->apic_state, tpr);
}

#define VECTORING_INFO_VECTOR_MASK     0xff

/* TODO: taskswitch handling */
static void save_state_to_tss32(CPUState *cpu, struct x86_tss_segment32 *tss)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    /* CR3 and ldt selector are not saved intentionally */
    tss->eip = EIP(env);
    tss->eflags = EFLAGS(env);
    tss->eax = EAX(env);
    tss->ecx = ECX(env);
    tss->edx = EDX(env);
    tss->ebx = EBX(env);
    tss->esp = ESP(env);
    tss->ebp = EBP(env);
    tss->esi = ESI(env);
    tss->edi = EDI(env);

    tss->es = vmx_read_segment_selector(cpu, REG_SEG_ES).sel;
    tss->cs = vmx_read_segment_selector(cpu, REG_SEG_CS).sel;
    tss->ss = vmx_read_segment_selector(cpu, REG_SEG_SS).sel;
    tss->ds = vmx_read_segment_selector(cpu, REG_SEG_DS).sel;
    tss->fs = vmx_read_segment_selector(cpu, REG_SEG_FS).sel;
    tss->gs = vmx_read_segment_selector(cpu, REG_SEG_GS).sel;
}

static void load_state_from_tss32(CPUState *cpu, struct x86_tss_segment32 *tss)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    wvmcs(cpu->hvf_fd, VMCS_GUEST_CR3, tss->cr3);

    RIP(env) = tss->eip;
    EFLAGS(env) = tss->eflags | 2;

    /* General purpose registers */
    RAX(env) = tss->eax;
    RCX(env) = tss->ecx;
    RDX(env) = tss->edx;
    RBX(env) = tss->ebx;
    RSP(env) = tss->esp;
    RBP(env) = tss->ebp;
    RSI(env) = tss->esi;
    RDI(env) = tss->edi;

    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->ldt}},
                               REG_SEG_LDTR);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->es}},
                               REG_SEG_ES);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->cs}},
                               REG_SEG_CS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->ss}},
                               REG_SEG_SS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->ds}},
                               REG_SEG_DS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->fs}},
                               REG_SEG_FS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){{tss->gs}},
                               REG_SEG_GS);

#if 0
    load_segment(cpu, REG_SEG_LDTR, tss->ldt);
    load_segment(cpu, REG_SEG_ES, tss->es);
    load_segment(cpu, REG_SEG_CS, tss->cs);
    load_segment(cpu, REG_SEG_SS, tss->ss);
    load_segment(cpu, REG_SEG_DS, tss->ds);
    load_segment(cpu, REG_SEG_FS, tss->fs);
    load_segment(cpu, REG_SEG_GS, tss->gs);
#endif
}

static int task_switch_32(CPUState *cpu, x68_segment_selector tss_sel,
                          x68_segment_selector old_tss_sel,
                          uint64_t old_tss_base,
                          struct x86_segment_descriptor *new_desc)
{
    struct x86_tss_segment32 tss_seg;
    uint32_t new_tss_base = x86_segment_base(new_desc);
    uint32_t eip_offset = offsetof(struct x86_tss_segment32, eip);
    uint32_t ldt_sel_offset = offsetof(struct x86_tss_segment32, ldt);

    vmx_read_mem(cpu, &tss_seg, old_tss_base, sizeof(tss_seg));
    save_state_to_tss32(cpu, &tss_seg);

    vmx_write_mem(cpu, old_tss_base + eip_offset, &tss_seg.eip, ldt_sel_offset -
                  eip_offset);
    vmx_read_mem(cpu, &tss_seg, new_tss_base, sizeof(tss_seg));

    if (old_tss_sel.sel != 0xffff) {
        tss_seg.prev_tss = old_tss_sel.sel;

        vmx_write_mem(cpu, new_tss_base, &tss_seg.prev_tss,
                      sizeof(tss_seg.prev_tss));
    }
    load_state_from_tss32(cpu, &tss_seg);
    return 0;
}

static void vmx_handle_task_switch(CPUState *cpu, x68_segment_selector tss_sel,
        int reason, bool gate_valid, uint8_t gate, uint64_t gate_type)
{
    uint64_t rip = rreg(cpu->hvf_fd, HV_X86_RIP);
    if (!gate_valid || (gate_type != VMCS_INTR_T_HWEXCEPTION &&
                        gate_type != VMCS_INTR_T_HWINTR &&
                        gate_type != VMCS_INTR_T_NMI)) {
        int ins_len = rvmcs(cpu->hvf_fd, VMCS_EXIT_INSTRUCTION_LENGTH);
        macvm_set_rip(cpu, rip + ins_len);
        return;
    }

    load_regs(cpu);

    struct x86_segment_descriptor curr_tss_desc, next_tss_desc;
    int ret;
    x68_segment_selector old_tss_sel = vmx_read_segment_selector(cpu, REG_SEG_TR);
    uint64_t old_tss_base = vmx_read_segment_base(cpu, REG_SEG_TR);
    uint32_t desc_limit;
    struct x86_call_gate task_gate_desc;
    struct vmx_segment vmx_seg;
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    x86_read_segment_descriptor(cpu, &next_tss_desc, tss_sel);
    x86_read_segment_descriptor(cpu, &curr_tss_desc, old_tss_sel);

    if (reason == TSR_IDT_GATE && gate_valid) {
        int dpl;

        ret = x86_read_call_gate(cpu, &task_gate_desc, gate);

        dpl = task_gate_desc.dpl;
        x68_segment_selector cs = vmx_read_segment_selector(cpu, REG_SEG_CS);
        if (tss_sel.rpl > dpl || cs.rpl > dpl) {
            VM_PANIC("emulate_gp");
        }
    }

    desc_limit = x86_segment_limit(&next_tss_desc);
    if (!next_tss_desc.p || ((desc_limit < 0x67 && (next_tss_desc.type & 8)) ||
        desc_limit < 0x2b)) {
        VM_PANIC("emulate_ts");
    }

    if (reason == TSR_IRET || reason == TSR_JMP) {
        curr_tss_desc.type &= ~(1 << 1); /* clear busy flag */
        x86_write_segment_descriptor(cpu, &curr_tss_desc, old_tss_sel);
    }

    if (reason == TSR_IRET) {
        EFLAGS(env) &= ~RFLAGS_NT;
    }

    if (reason != TSR_CALL && reason != TSR_IDT_GATE) {
        old_tss_sel.sel = 0xffff;
    }

    if (reason != TSR_IRET) {
        next_tss_desc.type |= (1 << 1); /* set busy flag */
        x86_write_segment_descriptor(cpu, &next_tss_desc, tss_sel);
    }

    if (next_tss_desc.type & 8) {
        ret = task_switch_32(cpu, tss_sel, old_tss_sel, old_tss_base,
                             &next_tss_desc);
    } else {
        /*ret = task_switch_16(cpu, tss_sel, old_tss_sel, old_tss_base,
         * &next_tss_desc);*/
        VM_PANIC("task_switch_16");
    }

    macvm_set_cr0(cpu->hvf_fd, rvmcs(cpu->hvf_fd, VMCS_GUEST_CR0) | CR0_TS);
    x86_segment_descriptor_to_vmx(cpu, tss_sel, &next_tss_desc, &vmx_seg);
    vmx_write_segment_descriptor(cpu, &vmx_seg, REG_SEG_TR);

    store_regs(cpu);

    hv_vcpu_invalidate_tlb(cpu->hvf_fd);
    hv_vcpu_flush(cpu->hvf_fd);
}

static void hvf_handle_interrupt(CPUState *cpu, int mask)
{
    cpu->interrupt_request |= mask;
    if (!qemu_cpu_is_self(cpu)) {
        qemu_cpu_kick(cpu);
    }
}

void hvf_handle_io(CPUArchState *env, uint16_t port, void *buffer,
                  int direction, int size, int count)
{
    int i;
    uint8_t *ptr = buffer;

    for (i = 0; i < count; i++) {
        address_space_rw(&address_space_io, port, MEMTXATTRS_UNSPECIFIED,
                         ptr, size,
                         direction);
        ptr += size;
    }
}

/* TODO: synchronize vcpu state */
static void do_hvf_cpu_synchronize_state(CPUState *cpu, run_on_cpu_data arg)
{
    CPUState *cpu_state = cpu;
    if (cpu_state->vcpu_dirty == 0) {
        hvf_get_registers(cpu_state);
    }

    cpu_state->vcpu_dirty = 1;
}

void hvf_cpu_synchronize_state(CPUState *cpu_state)
{
    if (cpu_state->vcpu_dirty == 0) {
        run_on_cpu(cpu_state, do_hvf_cpu_synchronize_state, RUN_ON_CPU_NULL);
    }
}

static void do_hvf_cpu_synchronize_post_reset(CPUState *cpu, run_on_cpu_data arg)
{
    CPUState *cpu_state = cpu;
    hvf_put_registers(cpu_state);
    cpu_state->vcpu_dirty = false;
}

void hvf_cpu_synchronize_post_reset(CPUState *cpu_state)
{
    run_on_cpu(cpu_state, do_hvf_cpu_synchronize_post_reset, RUN_ON_CPU_NULL);
}

void _hvf_cpu_synchronize_post_init(CPUState *cpu, run_on_cpu_data arg)
{
    CPUState *cpu_state = cpu;
    hvf_put_registers(cpu_state);
    cpu_state->vcpu_dirty = false;
}

void hvf_cpu_synchronize_post_init(CPUState *cpu_state)
{
    run_on_cpu(cpu_state, _hvf_cpu_synchronize_post_init, RUN_ON_CPU_NULL);
}

static bool ept_emulation_fault(hvf_slot *slot, addr_t gpa, uint64_t ept_qual)
{
    int read, write;

    /* EPT fault on an instruction fetch doesn't make sense here */
    if (ept_qual & EPT_VIOLATION_INST_FETCH) {
        return false;
    }

    /* EPT fault must be a read fault or a write fault */
    read = ept_qual & EPT_VIOLATION_DATA_READ ? 1 : 0;
    write = ept_qual & EPT_VIOLATION_DATA_WRITE ? 1 : 0;
    if ((read | write) == 0) {
        return false;
    }

    if (write && slot) {
        if (slot->flags & HVF_SLOT_LOG) {
            memory_region_set_dirty(slot->region, gpa - slot->start, 1);
            hv_vm_protect((hv_gpaddr_t)slot->start, (size_t)slot->size,
                          HV_MEMORY_READ | HV_MEMORY_WRITE);
        }
    }

    /*
     * The EPT violation must have been caused by accessing a
     * guest-physical address that is a translation of a guest-linear
     * address.
     */
    if ((ept_qual & EPT_VIOLATION_GLA_VALID) == 0 ||
        (ept_qual & EPT_VIOLATION_XLAT_VALID) == 0) {
        return false;
    }

    return !slot;
}

static void hvf_set_dirty_tracking(MemoryRegionSection *section, bool on)
{
    struct mac_slot *macslot;
    hvf_slot *slot;

    slot = hvf_find_overlap_slot(
            section->offset_within_address_space,
            section->offset_within_address_space + int128_get64(section->size));

    /* protect region against writes; begin tracking it */
    if (on) {
        slot->flags |= HVF_SLOT_LOG;
        hv_vm_protect((hv_gpaddr_t)slot->start, (size_t)slot->size,
                      HV_MEMORY_READ);
    /* stop tracking region*/
    } else {
        slot->flags &= ~HVF_SLOT_LOG;
        hv_vm_protect((hv_gpaddr_t)slot->start, (size_t)slot->size,
                      HV_MEMORY_READ | HV_MEMORY_WRITE);
    }
}

static void hvf_log_start(MemoryListener *listener,
                          MemoryRegionSection *section, int old, int new)
{
    if (old != 0)
        return;

    hvf_set_dirty_tracking(section, 1);
}

static void hvf_log_stop(MemoryListener *listener,
                         MemoryRegionSection *section, int old, int new)
{
    if (new != 0)
        return;

    hvf_set_dirty_tracking(section, 0);
}

static void hvf_log_sync(MemoryListener *listener,
                         MemoryRegionSection *section)
{
    /*
     * sync of dirty pages is handled elsewhere; just make sure we keep
     * tracking the region.
     */
    hvf_set_dirty_tracking(section, 1);
}

static void hvf_region_add(MemoryListener *listener,
                           MemoryRegionSection *section)
{
    hvf_set_phys_mem(section, true);
}

static void hvf_region_del(MemoryListener *listener,
                           MemoryRegionSection *section)
{
    hvf_set_phys_mem(section, false);
}

static MemoryListener hvf_memory_listener = {
    .priority = 10,
    .region_add = hvf_region_add,
    .region_del = hvf_region_del,
    .log_start = hvf_log_start,
    .log_stop = hvf_log_stop,
    .log_sync = hvf_log_sync,
};

void vmx_reset_vcpu(CPUState *cpu) {

    /* TODO: this shouldn't be needed; there is already a call to
     * cpu_synchronize_all_post_reset in vl.c
     */
    wvmcs(cpu->hvf_fd, VMCS_ENTRY_CTLS, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_IA32_EFER, 0);
    macvm_set_cr0(cpu->hvf_fd, 0x60000010);
 
    wvmcs(cpu->hvf_fd, VMCS_CR4_MASK, CR4_VMXE_MASK);
    wvmcs(cpu->hvf_fd, VMCS_CR4_SHADOW, 0x0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CR4, CR4_VMXE_MASK);
 
    /* set VMCS guest state fields */
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CS_SELECTOR, 0xf000);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CS_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CS_ACCESS_RIGHTS, 0x9b);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CS_BASE, 0xffff0000);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_DS_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_DS_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_DS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_DS_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_ES_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_ES_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_ES_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_ES_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_FS_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_FS_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_FS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_FS_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GS_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GS_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GS_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_SS_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_SS_LIMIT, 0xffff);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_SS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_SS_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_LDTR_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_LDTR_LIMIT, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_LDTR_ACCESS_RIGHTS, 0x10000);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_LDTR_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_TR_SELECTOR, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_TR_LIMIT, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_TR_ACCESS_RIGHTS, 0x83);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_TR_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GDTR_LIMIT, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_GDTR_BASE, 0);
 
    wvmcs(cpu->hvf_fd, VMCS_GUEST_IDTR_LIMIT, 0);
    wvmcs(cpu->hvf_fd, VMCS_GUEST_IDTR_BASE, 0);
 
    /*wvmcs(cpu->hvf_fd, VMCS_GUEST_CR2, 0x0);*/
    wvmcs(cpu->hvf_fd, VMCS_GUEST_CR3, 0x0);
 
    wreg(cpu->hvf_fd, HV_X86_RIP, 0xfff0);
    wreg(cpu->hvf_fd, HV_X86_RDX, 0x623);
    wreg(cpu->hvf_fd, HV_X86_RFLAGS, 0x2);
    wreg(cpu->hvf_fd, HV_X86_RSP, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RAX, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RBX, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RCX, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RSI, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RDI, 0x0);
    wreg(cpu->hvf_fd, HV_X86_RBP, 0x0);
 
    for (int i = 0; i < 8; i++) {
        wreg(cpu->hvf_fd, HV_X86_R8 + i, 0x0);
    }

    hv_vm_sync_tsc(0);
    cpu->halted = 0;
    hv_vcpu_invalidate_tlb(cpu->hvf_fd);
    hv_vcpu_flush(cpu->hvf_fd);
}

void hvf_vcpu_destroy(CPUState *cpu)
{
    hv_return_t ret = hv_vcpu_destroy((hv_vcpuid_t)cpu->hvf_fd);
    assert_hvf_ok(ret);
}

static void dummy_signal(int sig)
{
}

int hvf_init_vcpu(CPUState *cpu)
{

    X86CPU *x86cpu = X86_CPU(cpu);
    CPUX86State *env = &x86cpu->env;

    /* init cpu signals */
    sigset_t set;
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = dummy_signal;
    sigaction(SIG_IPI, &sigact, NULL);

    pthread_sigmask(SIG_BLOCK, NULL, &set);
    sigdelset(&set, SIG_IPI);

    int r;
    init_emu();
    init_decoder();
    init_cpuid(cpu);

    hvf_state->hvf_caps = (struct hvf_vcpu_caps *)g_malloc0(sizeof(struct hvf_vcpu_caps));
    env->hvf_emul = (HVFX86EmulatorState *)g_malloc0(sizeof(HVFX86EmulatorState));

    r = hv_vcpu_create((hv_vcpuid_t *)&cpu->hvf_fd, HV_VCPU_DEFAULT);
    cpu->vcpu_dirty = 1;
    assert_hvf_ok(r);

    if (hv_vmx_read_capability(HV_VMX_CAP_PINBASED,
        &hvf_state->hvf_caps->vmx_cap_pinbased)) {
        abort();
    }
    if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED,
        &hvf_state->hvf_caps->vmx_cap_procbased)) {
        abort();
    }
    if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED2,
        &hvf_state->hvf_caps->vmx_cap_procbased2)) {
        abort();
    }
    if (hv_vmx_read_capability(HV_VMX_CAP_ENTRY,
        &hvf_state->hvf_caps->vmx_cap_entry)) {
        abort();
    }

    /* set VMCS control fields */
    wvmcs(cpu->hvf_fd, VMCS_PIN_BASED_CTLS,
          cap2ctrl(hvf_state->hvf_caps->vmx_cap_pinbased, 0));
    wvmcs(cpu->hvf_fd, VMCS_PRI_PROC_BASED_CTLS,
          cap2ctrl(hvf_state->hvf_caps->vmx_cap_procbased,
          VMCS_PRI_PROC_BASED_CTLS_HLT |
          VMCS_PRI_PROC_BASED_CTLS_MWAIT |
          VMCS_PRI_PROC_BASED_CTLS_TSC_OFFSET |
          VMCS_PRI_PROC_BASED_CTLS_TPR_SHADOW) |
          VMCS_PRI_PROC_BASED_CTLS_SEC_CONTROL);
    wvmcs(cpu->hvf_fd, VMCS_SEC_PROC_BASED_CTLS,
          cap2ctrl(hvf_state->hvf_caps->vmx_cap_procbased2,
                   VMCS_PRI_PROC_BASED2_CTLS_APIC_ACCESSES));

    wvmcs(cpu->hvf_fd, VMCS_ENTRY_CTLS, cap2ctrl(hvf_state->hvf_caps->vmx_cap_entry,
          0));
    wvmcs(cpu->hvf_fd, VMCS_EXCEPTION_BITMAP, 0); /* Double fault */

    wvmcs(cpu->hvf_fd, VMCS_TPR_THRESHOLD, 0);

    vmx_reset_vcpu(cpu);

    x86cpu = X86_CPU(cpu);
    x86cpu->env.kvm_xsave_buf = qemu_memalign(4096,
                                 sizeof(struct hvf_xsave_buf));

    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_STAR, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_LSTAR, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_CSTAR, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_FMASK, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_FSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_GSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_KERNELGSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_TSC_AUX, 1);
    /*hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_IA32_TSC, 1);*/
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_IA32_SYSENTER_CS, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_IA32_SYSENTER_EIP, 1);
    hv_vcpu_enable_native_msr(cpu->hvf_fd, MSR_IA32_SYSENTER_ESP, 1);

    return 0;
}

int hvf_enabled()
{
    return !hvf_disabled;
}

void hvf_disable(int shouldDisable)
{
    hvf_disabled = shouldDisable;
}

static void hvf_store_events(CPUState *cpu, uint32_t ins_len, uint64_t idtvec_info)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;

    env->exception_injected = -1;
    env->interrupt_injected = -1;
    env->nmi_injected = false;
    if (idtvec_info & VMCS_IDT_VEC_VALID) {
        switch (idtvec_info & VMCS_IDT_VEC_TYPE) {
        case VMCS_IDT_VEC_HWINTR:
        case VMCS_IDT_VEC_SWINTR:
            env->interrupt_injected = idtvec_info & VMCS_IDT_VEC_VECNUM;
            break;
        case VMCS_IDT_VEC_NMI:
            env->nmi_injected = true;
            break;
        case VMCS_IDT_VEC_HWEXCEPTION:
        case VMCS_IDT_VEC_SWEXCEPTION:
            env->exception_injected = idtvec_info & VMCS_IDT_VEC_VECNUM;
            break;
        case VMCS_IDT_VEC_PRIV_SWEXCEPTION:
        default:
            abort();
        }
        if ((idtvec_info & VMCS_IDT_VEC_TYPE) == VMCS_IDT_VEC_SWEXCEPTION ||
            (idtvec_info & VMCS_IDT_VEC_TYPE) == VMCS_IDT_VEC_SWINTR) {
            env->ins_len = ins_len;
        }
        if (idtvec_info & VMCS_INTR_DEL_ERRCODE) {
            env->has_error_code = true;
            env->error_code = rvmcs(cpu->hvf_fd, VMCS_IDT_VECTORING_ERROR);
        }
    }
    if ((rvmcs(cpu->hvf_fd, VMCS_GUEST_INTERRUPTIBILITY) &
        VMCS_INTERRUPTIBILITY_NMI_BLOCKING)) {
        env->hflags2 |= HF2_NMI_MASK;
    } else {
        env->hflags2 &= ~HF2_NMI_MASK;
    }
    if (rvmcs(cpu->hvf_fd, VMCS_GUEST_INTERRUPTIBILITY) &
         (VMCS_INTERRUPTIBILITY_STI_BLOCKING |
         VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)) {
        env->hflags |= HF_INHIBIT_IRQ_MASK;
    } else {
        env->hflags &= ~HF_INHIBIT_IRQ_MASK;
    }
}

int hvf_vcpu_exec(CPUState *cpu)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;
    int ret = 0;
    uint64_t rip = 0;

    cpu->halted = 0;

    if (hvf_process_events(cpu)) {
        return EXCP_HLT;
    }

    do {
        if (cpu->vcpu_dirty) {
            hvf_put_registers(cpu);
            cpu->vcpu_dirty = false;
        }

        hvf_inject_interrupts(cpu);
        vmx_update_tpr(cpu);


        qemu_mutex_unlock_iothread();
        if (!cpu_is_bsp(X86_CPU(cpu)) && cpu->halted) {
            qemu_mutex_lock_iothread();
            return EXCP_HLT;
        }

        hv_return_t r  = hv_vcpu_run(cpu->hvf_fd);
        assert_hvf_ok(r);

        /* handle VMEXIT */
        uint64_t exit_reason = rvmcs(cpu->hvf_fd, VMCS_EXIT_REASON);
        uint64_t exit_qual = rvmcs(cpu->hvf_fd, VMCS_EXIT_QUALIFICATION);
        uint32_t ins_len = (uint32_t)rvmcs(cpu->hvf_fd,
                                           VMCS_EXIT_INSTRUCTION_LENGTH);

        uint64_t idtvec_info = rvmcs(cpu->hvf_fd, VMCS_IDT_VECTORING_INFO);

        hvf_store_events(cpu, ins_len, idtvec_info);
        rip = rreg(cpu->hvf_fd, HV_X86_RIP);
        RFLAGS(env) = rreg(cpu->hvf_fd, HV_X86_RFLAGS);
        env->eflags = RFLAGS(env);

        qemu_mutex_lock_iothread();

        update_apic_tpr(cpu);
        current_cpu = cpu;

        ret = 0;
        switch (exit_reason) {
        case EXIT_REASON_HLT: {
            macvm_set_rip(cpu, rip + ins_len);
            if (!((cpu->interrupt_request & CPU_INTERRUPT_HARD) &&
                (EFLAGS(env) & IF_MASK))
                && !(cpu->interrupt_request & CPU_INTERRUPT_NMI) &&
                !(idtvec_info & VMCS_IDT_VEC_VALID)) {
                cpu->halted = 1;
                ret = EXCP_HLT;
            }
            ret = EXCP_INTERRUPT;
            break;
        }
        case EXIT_REASON_MWAIT: {
            ret = EXCP_INTERRUPT;
            break;
        }
            /* Need to check if MMIO or unmmaped fault */
        case EXIT_REASON_EPT_FAULT:
        {
            hvf_slot *slot;
            addr_t gpa = rvmcs(cpu->hvf_fd, VMCS_GUEST_PHYSICAL_ADDRESS);

            if (((idtvec_info & VMCS_IDT_VEC_VALID) == 0) &&
                ((exit_qual & EXIT_QUAL_NMIUDTI) != 0)) {
                vmx_set_nmi_blocking(cpu);
            }

            slot = hvf_find_overlap_slot(gpa, gpa);
            /* mmio */
            if (ept_emulation_fault(slot, gpa, exit_qual)) {
                struct x86_decode decode;

                load_regs(cpu);
                env->hvf_emul->fetch_rip = rip;

                decode_instruction(env, &decode);
                exec_instruction(env, &decode);
                store_regs(cpu);
                break;
            }
            break;
        }
        case EXIT_REASON_INOUT:
        {
            uint32_t in = (exit_qual & 8) != 0;
            uint32_t size =  (exit_qual & 7) + 1;
            uint32_t string =  (exit_qual & 16) != 0;
            uint32_t port =  exit_qual >> 16;
            /*uint32_t rep = (exit_qual & 0x20) != 0;*/

#if 1
            if (!string && in) {
                uint64_t val = 0;
                load_regs(cpu);
                hvf_handle_io(env, port, &val, 0, size, 1);
                if (size == 1) {
                    AL(env) = val;
                } else if (size == 2) {
                    AX(env) = val;
                } else if (size == 4) {
                    RAX(env) = (uint32_t)val;
                } else {
                    VM_PANIC("size");
                }
                RIP(env) += ins_len;
                store_regs(cpu);
                break;
            } else if (!string && !in) {
                RAX(env) = rreg(cpu->hvf_fd, HV_X86_RAX);
                hvf_handle_io(env, port, &RAX(env), 1, size, 1);
                macvm_set_rip(cpu, rip + ins_len);
                break;
            }
#endif
            struct x86_decode decode;

            load_regs(cpu);
            env->hvf_emul->fetch_rip = rip;

            decode_instruction(env, &decode);
            VM_PANIC_ON(ins_len != decode.len);
            exec_instruction(env, &decode);
            store_regs(cpu);

            break;
        }
        case EXIT_REASON_CPUID: {
            uint32_t rax = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RAX);
            uint32_t rbx = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RBX);
            uint32_t rcx = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RCX);
            uint32_t rdx = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RDX);

            cpu_x86_cpuid(env, rax, rcx, &rax, &rbx, &rcx, &rdx);

            wreg(cpu->hvf_fd, HV_X86_RAX, rax);
            wreg(cpu->hvf_fd, HV_X86_RBX, rbx);
            wreg(cpu->hvf_fd, HV_X86_RCX, rcx);
            wreg(cpu->hvf_fd, HV_X86_RDX, rdx);

            macvm_set_rip(cpu, rip + ins_len);
            break;
        }
        case EXIT_REASON_XSETBV: {
            X86CPU *x86_cpu = X86_CPU(cpu);
            CPUX86State *env = &x86_cpu->env;
            uint32_t eax = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RAX);
            uint32_t ecx = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RCX);
            uint32_t edx = (uint32_t)rreg(cpu->hvf_fd, HV_X86_RDX);

            if (ecx) {
                macvm_set_rip(cpu, rip + ins_len);
                break;
            }
            env->xcr0 = ((uint64_t)edx << 32) | eax;
            wreg(cpu->hvf_fd, HV_X86_XCR0, env->xcr0 | 1);
            macvm_set_rip(cpu, rip + ins_len);
            break;
        }
        case EXIT_REASON_INTR_WINDOW:
            vmx_clear_int_window_exiting(cpu);
            ret = EXCP_INTERRUPT;
            break;
        case EXIT_REASON_NMI_WINDOW:
            vmx_clear_nmi_window_exiting(cpu);
            ret = EXCP_INTERRUPT;
            break;
        case EXIT_REASON_EXT_INTR:
            /* force exit and allow io handling */
            ret = EXCP_INTERRUPT;
            break;
        case EXIT_REASON_RDMSR:
        case EXIT_REASON_WRMSR:
        {
            load_regs(cpu);
            if (exit_reason == EXIT_REASON_RDMSR) {
                simulate_rdmsr(cpu);
            } else {
                simulate_wrmsr(cpu);
            }
            RIP(env) += rvmcs(cpu->hvf_fd, VMCS_EXIT_INSTRUCTION_LENGTH);
            store_regs(cpu);
            break;
        }
        case EXIT_REASON_CR_ACCESS: {
            int cr;
            int reg;

            load_regs(cpu);
            cr = exit_qual & 15;
            reg = (exit_qual >> 8) & 15;

            switch (cr) {
            case 0x0: {
                macvm_set_cr0(cpu->hvf_fd, RRX(env, reg));
                break;
            }
            case 4: {
                macvm_set_cr4(cpu->hvf_fd, RRX(env, reg));
                break;
            }
            case 8: {
                X86CPU *x86_cpu = X86_CPU(cpu);
                if (exit_qual & 0x10) {
                    RRX(env, reg) = cpu_get_apic_tpr(x86_cpu->apic_state);
                } else {
                    int tpr = RRX(env, reg);
                    cpu_set_apic_tpr(x86_cpu->apic_state, tpr);
                    ret = EXCP_INTERRUPT;
                }
                break;
            }
            default:
                fprintf(stderr, "Unrecognized CR %d\n", cr);
                abort();
            }
            RIP(env) += ins_len;
            store_regs(cpu);
            break;
        }
        case EXIT_REASON_APIC_ACCESS: { /* TODO */
            struct x86_decode decode;

            load_regs(cpu);
            env->hvf_emul->fetch_rip = rip;

            decode_instruction(env, &decode);
            exec_instruction(env, &decode);
            store_regs(cpu);
            break;
        }
        case EXIT_REASON_TPR: {
            ret = 1;
            break;
        }
        case EXIT_REASON_TASK_SWITCH: {
            uint64_t vinfo = rvmcs(cpu->hvf_fd, VMCS_IDT_VECTORING_INFO);
            x68_segment_selector sel = {.sel = exit_qual & 0xffff};
            vmx_handle_task_switch(cpu, sel, (exit_qual >> 30) & 0x3,
             vinfo & VMCS_INTR_VALID, vinfo & VECTORING_INFO_VECTOR_MASK, vinfo
             & VMCS_INTR_T_MASK);
            break;
        }
        case EXIT_REASON_TRIPLE_FAULT: {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            ret = EXCP_INTERRUPT;
            break;
        }
        case EXIT_REASON_RDPMC:
            wreg(cpu->hvf_fd, HV_X86_RAX, 0);
            wreg(cpu->hvf_fd, HV_X86_RDX, 0);
            macvm_set_rip(cpu, rip + ins_len);
            break;
        case VMX_REASON_VMCALL:
            env->exception_injected = EXCP0D_GPF;
            env->has_error_code = true;
            env->error_code = 0;
            break;
        default:
            fprintf(stderr, "%llx: unhandled exit %llx\n", rip, exit_reason);
        }
    } while (ret == 0);

    return ret;
}

static bool hvf_allowed;

static int hvf_accel_init(MachineState *ms)
{
    int x;
    hv_return_t ret;
    HVFState *s;

    ret = hv_vm_create(HV_VM_DEFAULT);
    assert_hvf_ok(ret);

    s = (HVFState *)g_malloc0(sizeof(HVFState));

    s->num_slots = 32;
    for (x = 0; x < s->num_slots; ++x) {
        s->slots[x].size = 0;
        s->slots[x].slot_id = x;
    }

    hvf_state = s;
    cpu_interrupt_handler = hvf_handle_interrupt;
    memory_listener_register(&hvf_memory_listener, &address_space_memory);
    return 0;
}

static void hvf_accel_class_init(ObjectClass *oc, void *data)
{
    AccelClass *ac = ACCEL_CLASS(oc);
    ac->name = "HVF";
    ac->init_machine = hvf_accel_init;
    ac->allowed = &hvf_allowed;
}

static const TypeInfo hvf_accel_type = {
    .name = TYPE_HVF_ACCEL,
    .parent = TYPE_ACCEL,
    .class_init = hvf_accel_class_init,
};

static void hvf_type_init(void)
{
    type_register_static(&hvf_accel_type);
}

type_init(hvf_type_init);
