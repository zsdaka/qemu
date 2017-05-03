#ifndef SYSEMU_NUMA_H
#define SYSEMU_NUMA_H

#include "qemu/bitmap.h"
#include "qemu/option.h"
#include "sysemu/sysemu.h"
#include "sysemu/hostmem.h"
#include "hw/boards.h"

extern int nb_numa_nodes;   /* Number of NUMA nodes */

struct numa_addr_range {
    ram_addr_t mem_start;
    ram_addr_t mem_end;
    QLIST_ENTRY(numa_addr_range) entry;
};

typedef struct node_info {
    uint64_t node_mem;
    unsigned long *node_cpu;
    struct HostMemoryBackend *node_memdev;
    bool present;
    QLIST_HEAD(, numa_addr_range) addr; /* List to store address ranges */
} NodeInfo;

extern NodeInfo numa_info[MAX_NODES];
void parse_numa_opts(MachineState *ms);
void numa_post_machine_init(void);
void query_numa_node_mem(uint64_t node_mem[]);
extern QemuOptsList qemu_numa_opts;
void numa_set_mem_node_id(ram_addr_t addr, uint64_t size, uint32_t node);
void numa_unset_mem_node_id(ram_addr_t addr, uint64_t size, uint32_t node);
uint32_t numa_get_node(ram_addr_t addr, Error **errp);

static inline bool numa_has_node_id(const CPUArchIdList *possible_cpus, int idx)
{
    return possible_cpus->cpus[idx].props.has_node_id;
}

static inline int numa_node_id(const CPUArchIdList *possible_cpus, int idx)
{
    return possible_cpus->cpus[idx].props.node_id;
}
#endif
