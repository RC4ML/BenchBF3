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

#include "utils_common.h"

DOCA_LOG_REGISTER(UTILS_COMMON);

doca_error_t
program_core_objects::init_core_objects(uint32_t max_bufs, DOCA::engine_to_ctx *engine_ctx) {
    try {
        (void)engine_ctx;
        src_mmap = new DOCA::mmap(true);

        src_mmap->add_device(dev);

        dst_mmap = new DOCA::mmap(true);

        dst_mmap->add_device(dev);

        buf_inv = new DOCA::buf_inventory(max_bufs);

        pe = new DOCA::pe();
        pe->connect_ctx(ctx);
    }
    catch (DOCA::DOCAException &e) {
        DOCA_LOG_ERR("Failed to create core objects: %s", e.what());
        return e.get_error();
    }
    return DOCA_SUCCESS;
}

void program_core_objects::clean_all() const {

    delete buf_inv;

    delete pe;

    delete src_mmap;

    delete dst_mmap;

    delete dev;
}

char *
hex_dump(const void *data, size_t size) {
    /*
     * <offset>:     <Hex bytes: 1-8>        <Hex bytes: 9-16>         <Ascii>
     * 00000000: 31 32 33 34 35 36 37 38  39 30 61 62 63 64 65 66  1234567890abcdef
     *    8     2         8 * 3          1          8 * 3         1       16       1
     */
    const size_t line_size = 8 + 2 + 8 * 3 + 1 + 8 * 3 + 1 + 16 + 1;
    size_t i, j, r, read_index;
    size_t num_lines, buffer_size;
    char *buffer, *write_head;
    unsigned char cur_char, printable;
    char ascii_line[17];
    const unsigned char *input_buffer;

    /* Allocate a dynamic buffer to hold the full result */
    num_lines = (size + 16 - 1) / 16;
    buffer_size = num_lines * line_size + 1;
    buffer = static_cast<char *>(malloc(buffer_size));
    if (buffer == nullptr)
        return nullptr;
    write_head = buffer;
    input_buffer = static_cast<const unsigned char *>(data);
    read_index = 0;

    for (i = 0; i < num_lines; i++) {
        /* Offset */
        snprintf(write_head, buffer_size, "%08lX: ", i * 16);
        write_head += 8 + 2;
        buffer_size -= 8 + 2;
        /* Hex print - 2 chunks of 8 bytes */
        for (r = 0; r < 2; r++) {
            for (j = 0; j < 8; j++) {
                /* If there is content to print */
                if (read_index < size) {
                    cur_char = input_buffer[read_index++];
                    snprintf(write_head, buffer_size, "%02X ", cur_char);
                    /* Printable chars go "as-is" */
                    if (' ' <= cur_char && cur_char <= '~')
                        printable = cur_char;
                    /* Otherwise, use a '.' */
                    else
                        printable = '.';
                    /* Else, just use spaces */
                } else {
                    snprintf(write_head, buffer_size, "   ");
                    printable = ' ';
                }
                ascii_line[r * 8 + j] = static_cast<char>(printable);
                write_head += 3;
                buffer_size -= 3;
            }
            /* Spacer between the 2 hex groups */
            snprintf(write_head, buffer_size, " ");
            write_head += 1;
            buffer_size -= 1;
        }
        /* Ascii print */
        ascii_line[16] = '\0';
        snprintf(write_head, buffer_size, "%s\n", ascii_line);
        write_head += 16 + 1;
        buffer_size -= 16 + 1;
    }
    /* No need for the last '\n' */
    write_head[-1] = '\0';
    return buffer;
}

std::string get_file_contents(const char *filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return (contents);
    }
    throw (errno);
}

std::vector<std::string> get_files_in_directory(const std::string &directory) {
    std::vector<std::string> files;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {  // 只添加文件，不包括子目录
            files.push_back(entry.path().string());
        }
    }
    return files;
}