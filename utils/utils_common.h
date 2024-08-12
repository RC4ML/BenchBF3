/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_device_rep.h"
#include "wrapper_dma.h"
#include "wrapper_interface.h"
#include "wrapper_mmap.h"
#include "wrapper_pe.h"
#include "proto/dma_connect.pb.h"

#define PCI_ADDR_SIZE 8 /* Null terminator included */


 /* DOCA core objects used by the samples / applications */
class program_core_objects {
public:
    DOCA::dev *dev;                  /* doca device */
    DOCA::mmap *src_mmap;          /* doca mmap for source buffer */
    DOCA::mmap *dst_mmap;          /* doca mmap for destination buffer */
    DOCA::buf_inventory *buf_inv; /* doca buffer inventory */
    DOCA::ctx *ctx;                  /* doca context */
    DOCA::pe *pe;              /* doca work queue */

    doca_error_t init_core_objects(uint32_t max_bufs, DOCA::engine_to_ctx *engine_ctx);

    void clean_all() const;
};

class doca_conn_info {
public:
    std::vector<std::string> exports;
    std::vector<std::pair<uint64_t, size_t>> buffers;

    static dma_connect::doca_conn_info to_protobuf(const doca_conn_info &info) {
        dma_connect::doca_conn_info conn_info_inner;
        for (auto &e : info.exports) {
            conn_info_inner.add_exports(e);
        }

        for (auto &p : info.buffers) {
            conn_info_inner.add_buffers_ptr(p.first);
            conn_info_inner.add_buffers_size(p.second);
        }
        return conn_info_inner;
    }

    static doca_conn_info from_protobuf(const dma_connect::doca_conn_info &info) {
        doca_conn_info conn_info_inner;
        for (int i = 0; i < info.exports_size(); i++) {
            conn_info_inner.exports.push_back(info.exports(i));
        }

        for (int i = 0; i < info.buffers_ptr_size(); i++) {
            conn_info_inner.buffers.emplace_back(info.buffers_ptr(i), info.buffers_size(i));
        }
        return conn_info_inner;
    }
};


/*
 * Create a string Hex dump representation of the given input buffer
 *
 * @data [in]: Pointer to the input buffer
 * @size [in]: Number of bytes to be analyzed
 * @return: pointer to the string representation, or NULL if an error was encountered
 */
char *hex_dump(const void *data, size_t size);

std::string get_file_contents(const char *filename);

std::vector<std::string> get_files_in_directory(const std::string &directory);