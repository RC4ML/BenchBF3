#pragma once

#include <vector>
#include <thread>
#include <numaif.h>
#include <sys/shm.h>
#include <numa.h>

namespace DOCA {

    /// Return the number of logical cores per NUMA node
    size_t num_lcores_per_numa_node();

    /// Return a list of logical cores in \p numa_node
    std::vector<size_t> get_lcores_for_numa_node(size_t numa_node);

    /// Bind this thread to the core with index numa_local_index on the socket =
    /// numa_node
    void bind_to_core(std::thread &thread, size_t numa_node,
                      size_t numa_local_index);

    int get_2M_huagepages_free(size_t numa_node);

    int get_2M_huagepages_nr(size_t numa_node);

    void *get_huge_mem(uint32_t numa_node, size_t size);
}