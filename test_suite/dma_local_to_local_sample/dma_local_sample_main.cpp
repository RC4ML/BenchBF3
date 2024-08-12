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

#include <doca_log.h>
#include "gflags_common.h"
#include "utils_common.h"

DOCA_LOG_REGISTER(DMA_LOCAL_SAMPLE_MAIN);

/* Sample's Logic */
doca_error_t dma_local_copy(std::string &pci_addr, char *dst_buffer, char *src_buffer, size_t length);

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int main(int argc, char **argv) {
    char *dst_buffer, *src_buffer;
    size_t length;
    doca_error_t result;

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // #ifndef DOCA_ARCH_DPU
    // 	DOCA_LOG_ERR("Local DMA copy can run only on the DPU");
    // 	doca_argp_destroy();
    // 	return EXIT_FAILURE;
    // #endif
    length = 128;

    DOCA::rt_assert(length % 64 == 0, "Length must be a multiple of 64 bytes");

    dst_buffer = reinterpret_cast<char *>(calloc(1, length));
    if (dst_buffer == nullptr) {
        DOCA_LOG_ERR("Destination buffer allocation failed");
        return EXIT_FAILURE;
    }

    src_buffer = reinterpret_cast<char *>(malloc(length));
    if (src_buffer == nullptr) {
        DOCA_LOG_ERR("Source buffer allocation failed");
        free(dst_buffer);
        return EXIT_FAILURE;
    }

    memcpy(src_buffer, FLAGS_g_sample_text.c_str(), length);
    for (size_t i = 0;i < length;i++) {
        src_buffer[i] = 'a';
    }

    result = dma_local_copy(FLAGS_g_pci_address, dst_buffer, src_buffer, length);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Sample function has failed: %s", doca_error_get_descr(result));
        free(src_buffer);
        free(dst_buffer);
        return EXIT_FAILURE;
    }

    free(src_buffer);
    free(dst_buffer);

    return EXIT_SUCCESS;
}
