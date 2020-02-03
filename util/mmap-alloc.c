/*
 * Support for RAM backed by mmaped host memory.
 *
 * Copyright (c) 2015 Red Hat, Inc.
 *
 * Authors:
 *  Michael S. Tsirkin <mst@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#ifdef CONFIG_LINUX
#include <linux/mman.h>
#else  /* !CONFIG_LINUX */
#define MAP_SYNC              0x0
#define MAP_SHARED_VALIDATE   0x0
#endif /* CONFIG_LINUX */

#include "qemu/osdep.h"
#include "qemu/mmap-alloc.h"
#include "qemu/host-utils.h"

#define HUGETLBFS_MAGIC       0x958458f6

#ifdef CONFIG_LINUX
#include <sys/vfs.h>
#endif

size_t qemu_fd_getpagesize(int fd)
{
#ifdef CONFIG_LINUX
    struct statfs fs;
    int ret;

    if (fd != -1) {
        do {
            ret = fstatfs(fd, &fs);
        } while (ret != 0 && errno == EINTR);

        if (ret == 0 && fs.f_type == HUGETLBFS_MAGIC) {
            return fs.f_bsize;
        }
    }
#ifdef __sparc__
    /* SPARC Linux needs greater alignment than the pagesize */
    return QEMU_VMALLOC_ALIGN;
#endif
#endif

    return qemu_real_host_page_size;
}

size_t qemu_mempath_getpagesize(const char *mem_path)
{
#ifdef CONFIG_LINUX
    struct statfs fs;
    int ret;

    if (mem_path) {
        do {
            ret = statfs(mem_path, &fs);
        } while (ret != 0 && errno == EINTR);

        if (ret != 0) {
            fprintf(stderr, "Couldn't statfs() memory path: %s\n",
                    strerror(errno));
            exit(1);
        }

        if (fs.f_type == HUGETLBFS_MAGIC) {
            /* It's hugepage, return the huge page size */
            return fs.f_bsize;
        }
    }
#ifdef __sparc__
    /* SPARC Linux needs greater alignment than the pagesize */
    return QEMU_VMALLOC_ALIGN;
#endif
#endif

    return qemu_real_host_page_size;
}

/*
 * Reserve a new memory region of the requested size to be used for mapping
 * from the given fd (if any).
 */
static void *mmap_reserve(size_t size, int fd)
{
    int flags = MAP_PRIVATE;

#if defined(__powerpc64__) && defined(__linux__)
    /*
     * On ppc64 mappings in the same segment (aka slice) must share the same
     * page size. Since we will be re-allocating part of this segment
     * from the supplied fd, we should make sure to use the same page size, to
     * this end we mmap the supplied fd.  In this case, set MAP_NORESERVE to
     * avoid allocating backing store memory.
     * We do this unless we are using the system page size, in which case
     * anonymous memory is OK.
     */
    if (fd == -1 || qemu_fd_getpagesize(fd) == qemu_real_host_page_size) {
        fd = -1;
        flags |= MAP_ANONYMOUS;
    } else {
        flags |= MAP_NORESERVE;
    }
#else
    fd = -1;
    flags |= MAP_ANONYMOUS;
#endif

    return mmap(0, size, PROT_NONE, flags, fd, 0);
}

/*
 * Populate memory in a reserved region from the given fd (if any).
 */
static void *mmap_populate(void *ptr, size_t size, int fd, bool shared,
                           bool is_pmem)
{
    int map_sync_flags = 0;
    int flags = MAP_FIXED;
    void *new_ptr;

    flags |= fd == -1 ? MAP_ANONYMOUS : 0;
    flags |= shared ? MAP_SHARED : MAP_PRIVATE;
    if (shared && is_pmem) {
        map_sync_flags = MAP_SYNC | MAP_SHARED_VALIDATE;
    }

    new_ptr = mmap(ptr, size, PROT_READ | PROT_WRITE, flags | map_sync_flags,
                   fd, 0);
    if (new_ptr == MAP_FAILED && map_sync_flags) {
        if (errno == ENOTSUP) {
            char *proc_link = g_strdup_printf("/proc/self/fd/%d", fd);
            char *file_name = g_malloc0(PATH_MAX);
            int len = readlink(proc_link, file_name, PATH_MAX - 1);

            if (len < 0) {
                len = 0;
            }
            file_name[len] = '\0';
            fprintf(stderr, "Warning: requesting persistence across crashes "
                    "for backend file %s failed. Proceeding without "
                    "persistence, data might become corrupted in case of host "
                    "crash.\n", file_name);
            g_free(proc_link);
            g_free(file_name);
        }
        /*
         * If mmap failed with MAP_SHARED_VALIDATE | MAP_SYNC, we will try
         * again without these flags to handle backwards compatibility.
         */
        new_ptr = mmap(ptr, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    }
    return new_ptr;
}

static inline size_t mmap_pagesize(int fd)
{
#if defined(__powerpc64__) && defined(__linux__)
    /* Mappings in the same segment must share the same page size */
    return qemu_fd_getpagesize(fd);
#else
    return qemu_real_host_page_size;
#endif
}

void *qemu_ram_mmap(int fd,
                    size_t size,
                    size_t align,
                    bool shared,
                    bool is_pmem)
{
    const size_t pagesize = mmap_pagesize(fd);
    size_t offset, total;
    void *ptr, *guardptr;

    /*
     * Note: this always allocates at least one extra page of virtual address
     * space, even if size is already aligned.
     */
    total = size + align;

    guardptr = mmap_reserve(total, fd);
    if (guardptr == MAP_FAILED) {
        return MAP_FAILED;
    }

    assert(is_power_of_2(align));
    /* Always align to host page size */
    assert(align >= pagesize);

    offset = QEMU_ALIGN_UP((uintptr_t)guardptr, align) - (uintptr_t)guardptr;

    ptr = mmap_populate(guardptr + offset, size, fd, shared, is_pmem);
    if (ptr == MAP_FAILED) {
        munmap(guardptr, total);
        return MAP_FAILED;
    }

    if (offset > 0) {
        munmap(guardptr, offset);
    }

    /*
     * Leave a single PROT_NONE page allocated after the RAM block, to serve as
     * a guard page guarding against potential buffer overflows.
     */
    total -= offset;
    if (total > size + pagesize) {
        munmap(ptr + size + pagesize, total - size - pagesize);
    }

    return ptr;
}

void qemu_ram_munmap(int fd, void *ptr, size_t size)
{
    const size_t pagesize = mmap_pagesize(fd);

    if (ptr) {
        /* Unmap both the RAM block and the guard page */
        munmap(ptr, size + pagesize);
    }
}
