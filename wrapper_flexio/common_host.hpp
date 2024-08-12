#pragma once

#include <stdint.h>

struct mlx5_psv {
    uint32_t index;
    struct mlx5dv_devx_obj *devx_obj;
};
enum mlx5_sig_type {
    MLX5_SIG_TYPE_NONE = 0,
    MLX5_SIG_TYPE_CRC,
    MLX5_SIG_TYPE_T10DIF,
};
enum mlx5_mkey_bsf_state {
    MLX5_MKEY_BSF_STATE_INIT,
    MLX5_MKEY_BSF_STATE_RESET,
    MLX5_MKEY_BSF_STATE_SET,
    MLX5_MKEY_BSF_STATE_UPDATED,
};
struct mlx5_sig_err {
    uint16_t syndrome;
    uint64_t expected;
    uint64_t actual;
    uint64_t offset;
    uint8_t sig_type;
    uint8_t domain;
};
struct mlx5_sig_block_domain {
    enum mlx5_sig_type sig_type;
    union {
        struct mlx5dv_sig_t10dif dif;
        struct mlx5dv_sig_crc crc;
    } sig;
    enum mlx5dv_block_size block_size;
};
struct mlx5_sig_block_attr {
    struct mlx5_sig_block_domain mem;
    struct mlx5_sig_block_domain wire;
    uint32_t flags;
    uint8_t check_mask;
    uint8_t copy_mask;
};
struct mlx5_sig_block {
    struct mlx5_psv *mem_psv;
    struct mlx5_psv *wire_psv;
    struct mlx5_sig_block_attr attr;
    enum mlx5_mkey_bsf_state state;
};
struct mlx5_sig_ctx {
    struct mlx5_sig_block block;
    struct mlx5_sig_err err_info;
    uint32_t err_count;
    bool err_exists;
    bool err_count_updated;
};
struct mlx5_mkey {
    struct mlx5dv_mkey dv_mkey;
    struct mlx5dv_devx_obj *devx_obj;
    uint16_t num_desc;
    uint64_t length;
    struct mlx5_sig_ctx *sig;
    struct mlx5_crypto_attr *crypto;
};

