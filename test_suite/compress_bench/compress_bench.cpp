#include "compress_bench.h"
#include "utils_common.h"
#include <zlib.h>
#include <lz4.h>

DOCA_LOG_REGISTER(COMPRESS_BENCH);

DEFINE_string(input_path, "", "test file dir path");

#define PAGE_SIZE (sysconf(_SC_PAGESIZE)) /* OS page size */

static doca_error_t
compress_file_deflate_sw(std::string &file_data, size_t dst_buf_size, uint8_t **compressed_file,
                         size_t *compressed_file_len) {
    z_stream c_stream;
    int err;
    memset(&c_stream, 0, sizeof(c_stream));

    c_stream.zalloc = nullptr;
    c_stream.zfree = nullptr;
    err = deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    c_stream.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(file_data.c_str()));
    c_stream.next_out = *compressed_file;

    c_stream.avail_in = file_data.size();
    c_stream.avail_out = dst_buf_size;

    err = deflate(&c_stream, Z_NO_FLUSH);
    if (err < 0) {
        DOCA_LOG_ERR("Failed to compress file");
        return DOCA_ERROR_BAD_STATE;
    }
    err = deflate(&c_stream, Z_FINISH);
    if (err < 0 || err != Z_STREAM_END) {
        DOCA_LOG_ERR("Failed to compress file");
        return DOCA_ERROR_BAD_STATE;
    }
    err = deflateEnd(&c_stream);
    if (err < 0) {
        DOCA_LOG_ERR("Failed to compress file");
        return DOCA_ERROR_BAD_STATE;
    }
    *compressed_file_len = c_stream.total_out;
    return DOCA_SUCCESS;
}

static doca_error_t
decompress_file_deflate_sw(uint8_t *file_data, size_t file_size, size_t dst_buf_size, uint8_t **decompressed_file,
                           size_t *decompressed_file_len) {
    z_stream d_stream;
    int err;
    memset(&d_stream, 0, sizeof(d_stream));

    d_stream.zalloc = nullptr;
    d_stream.zfree = nullptr;
    err = inflateInit2(&d_stream, -MAX_WBITS);
    d_stream.next_in = file_data;
    d_stream.next_out = *decompressed_file;

    d_stream.avail_in = file_size;
    d_stream.avail_out = dst_buf_size;

    err = inflate(&d_stream, Z_NO_FLUSH);
    if (err < 0) {
        DOCA_LOG_ERR("Failed to decompress file");
        return DOCA_ERROR_BAD_STATE;
    }
    err = inflate(&d_stream, Z_FINISH);
    if (err < 0 || err != Z_STREAM_END) {
        DOCA_LOG_ERR("Failed to decompress file");
        return DOCA_ERROR_BAD_STATE;
    }
    err = inflateEnd(&d_stream);
    if (err < 0) {
        DOCA_LOG_ERR("Failed to decompress file");
        return DOCA_ERROR_BAD_STATE;
    }
    *decompressed_file_len = d_stream.total_out;
    return DOCA_SUCCESS;
}

static doca_error_t
compress_file_lz4_sw(std::string &file_data, size_t dst_buf_size, uint8_t **compressed_file,
                     size_t *compressed_file_len) {
    int compressed_len = LZ4_compress_default(file_data.c_str(), reinterpret_cast<char *>(*compressed_file),
                                              file_data.size(), dst_buf_size);
    if (compressed_len <= 0) {
        DOCA_LOG_ERR("Failed to compress file");
        return DOCA_ERROR_BAD_STATE;
    }
    *compressed_file_len = static_cast<size_t>(compressed_len);
    return DOCA_SUCCESS;
}

static doca_error_t
decompress_file_lz4_sw(uint8_t *file_data, size_t file_size, size_t dst_buf_size, uint8_t **decompressed_file,
                       size_t *decompressed_file_len) {
    int decompressed_len = LZ4_decompress_safe(const_cast<const char *>(reinterpret_cast<char *>(file_data)),
                                               reinterpret_cast<char *>(*decompressed_file), file_size,
                                               dst_buf_size);
    if (decompressed_len < 0) {
        DOCA_LOG_ERR("Failed to decompress file");
        return DOCA_ERROR_BAD_STATE;
    }
    *decompressed_file_len = static_cast<size_t>(decompressed_len);
    (*decompressed_file)[decompressed_len] = '\0';
    return DOCA_SUCCESS;
}

doca_error_t compress_bench(compress_config &config, bool is_lz4) {
    std::vector<std::string> input_str_vec;
    std::vector<uint8_t *> compressed_file_vec;
    std::vector<size_t> compressed_file_len_vec;
    std::vector<uint8_t *> decompressed_file_vec;
    std::vector<size_t> decompressed_file_len_vec;

    doca_error_t err;
    auto files_path = get_files_in_directory(config.input_str);
    size_t payload = get_file_contents(files_path[0].c_str()).size() * 2;
    const size_t TEST_LOOP = 800000000 / payload;

    auto *compressed_buffer = new uint8_t[files_path.size() * payload];
    auto *decompressed_buffer = new uint8_t[files_path.size() * payload];

    for (size_t i = 0; i < files_path.size(); i++) {
        std::string file_str = get_file_contents(files_path[i].c_str());
        auto *compressed_file = compressed_buffer + i * payload;
        size_t compressed_file_len = 0;
        if (is_lz4) {
            err = compress_file_lz4_sw(file_str, payload, &compressed_file,
                                       &compressed_file_len);
        } else {
            err = compress_file_deflate_sw(file_str, payload, &compressed_file,
                                           &compressed_file_len);
        }

        if (err != DOCA_SUCCESS) {
            DOCA_LOG_ERR("compress file error");
            exit(1);
        }
        compressed_file_vec.push_back(compressed_file);
        compressed_file_len_vec.push_back(compressed_file_len);
        decompressed_file_vec.push_back(decompressed_buffer + i * payload);
        decompressed_file_len_vec.push_back(payload);

        input_str_vec.push_back(std::move(file_str));
    }

    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config.pci_address.c_str(), nullptr);
    auto *compress = new DOCA::compress();

    auto *ctx = new DOCA::ctx(compress, std::vector<DOCA::dev *>{dev});

    auto *buf_inv = new DOCA::buf_inventory(config.batch_size * 2);

    auto *src_mmap = new DOCA::mmap(true);
    auto *dst_mmap = new DOCA::mmap(true);

    src_mmap->add_device(dev);
    src_mmap->set_memrange_and_start(reinterpret_cast<char *>(compressed_buffer), files_path.size() * payload);

    dst_mmap->add_device(dev);
    dst_mmap->set_memrange_and_start(reinterpret_cast<char *>(decompressed_buffer), files_path.size() * payload);

    auto *workq = new DOCA::workq(config.batch_size, compress);

    DOCA::compress_deflate_job job_request(compress);
    // dirty but useful
    if (is_lz4) {
        job_request.get_inner_ptr()->base.type = DOCA_DECOMPRESS_LZ4_JOB;
    }
    std::vector<DOCA::buf *> src_buf_vec(config.batch_size, nullptr);
    std::vector<DOCA::buf *> dst_buf_vec(config.batch_size, nullptr);

    auto enqueue_job = [&](size_t i) {
        size_t mod_i = i % config.batch_size;
        i = i % files_path.size();
        auto *src_buf = DOCA::buf::buf_inventory_buf_by_data(buf_inv, src_mmap, compressed_file_vec[i],
                                                             compressed_file_len_vec[i]);
        auto dst_buf = DOCA::buf::buf_inventory_buf_by_addr(buf_inv, dst_mmap, decompressed_file_vec[i],
                                                            decompressed_file_len_vec[i]);

        src_buf_vec[mod_i] = src_buf;
        dst_buf_vec[mod_i] = dst_buf;

        job_request.set_src(src_buf);
        job_request.set_dst(dst_buf);

        doca_error_t result;
        if ((result = workq->submit(&job_request)) != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    };

    auto dequeue_job = [&](size_t i) {
        size_t mod_i = i % config.batch_size;
        doca_error_t result;
        ::doca_event event{};

        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        delete src_buf_vec[mod_i];
        delete dst_buf_vec[mod_i];
    };

    DOCA::Timer timer;
    DOCA::Timer total_timer;

    total_timer.tic();
    size_t total_index = 0;
    for (total_index = 0; total_index < config.batch_size; total_index++) {
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (; total_index < TEST_LOOP; total_index++) {
        dequeue_job(total_index - config.batch_size);
        timer.add_now_time();
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (size_t i = 0; i < config.batch_size; i++, total_index++) {
        dequeue_job(total_index - config.batch_size);
        timer.add_now_time();
    }
    size_t total_time_us = total_timer.toc();
    for (size_t i = 0; i < files_path.size(); i++) {
        const char *str1 = input_str_vec[i].c_str();
        char *str2 = reinterpret_cast<char *>(decompressed_file_vec[i]);
        size_t file_size = input_str_vec[i].size();
        for (size_t j = 0; j < file_size; j++) {
            if (str1[j] != str2[j]) {
                DOCA_LOG_ERR("Failed to decompress file %s, pos %ld", files_path[i].c_str(), j);
                exit(1);
            }
        }
    }

    std::string test_name;
    if (is_lz4) {
        test_name = "lz4";
    } else {
        test_name = "deflate";
    }
    std::cout
            << DOCA::string_format("hw %s latency %.4f us\n", test_name.c_str(),
                                   double(timer.get_now_timepoint()) / (TEST_LOOP * 1000));
    std::cout
            << DOCA::string_format("hw %s bandwidth %.4f MB/s\n", test_name.c_str(),
                                   double(payload * TEST_LOOP / 2) / (total_time_us));

    delete workq;
    delete buf_inv;
    delete ctx;
    delete src_mmap;
    delete dst_mmap;
    delete dev;
    delete compress;

    timer.reset();
    total_timer.reset();

    total_timer.tic();
    for (total_index = 0; total_index < TEST_LOOP; total_index++) {
        size_t now_index = total_index % files_path.size();
        size_t result_size = 0;
        timer.minus_now_time();
        if (is_lz4) {
            decompress_file_lz4_sw(compressed_file_vec[now_index], compressed_file_len_vec[now_index],
                                   decompressed_file_len_vec[now_index],
                                   &(decompressed_file_vec[now_index]), &result_size);
        } else {
            decompress_file_deflate_sw(compressed_file_vec[now_index], compressed_file_len_vec[now_index],
                                       decompressed_file_len_vec[now_index],
                                       &(decompressed_file_vec[now_index]), &result_size);
        }
        timer.add_now_time();
        DOCA::rt_assert(result_size == input_str_vec[now_index].size());
    }
    total_time_us = total_timer.toc();

    std::cout
            << DOCA::string_format("sw %s latency %.4f us\n", test_name.c_str(),
                                   double(timer.get_now_timepoint()) / (TEST_LOOP * 1000));
    std::cout
            << DOCA::string_format("sw %s bandwidth %.4f MB/s\n", test_name.c_str(),
                                   double(payload * TEST_LOOP / 2) / (total_time_us));

    delete[] compressed_buffer;
    delete[] decompressed_buffer;
    return DOCA_SUCCESS;
}
