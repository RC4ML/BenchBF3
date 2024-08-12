#pragma once

#include <gflags/gflags.h>

DECLARE_string(g_pci_address);

DECLARE_string(g_rep_pci_address);

DECLARE_string(g_ibdev_name);

DECLARE_string(g_sample_text);

DECLARE_string(g_network_address);

DECLARE_uint32(g_thread_num);

DECLARE_bool(g_is_exported);

DECLARE_bool(g_is_server);

DECLARE_bool(g_is_read);

DECLARE_uint32(g_payload);

DECLARE_uint32(g_batch_size);

DECLARE_uint32(g_mmap_length);

DECLARE_uint32(g_life_time);
