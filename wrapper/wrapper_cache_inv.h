#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"
#include "wrapper_buffer.h"
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>

namespace DOCA {
    class cache_inv {
    public:
        // dev_name is the ib device name, like mlx5_2 
        // use show_gids to check
        explicit cache_inv(std::string dev_name, DOCA::dev *dev);

        explicit cache_inv(std::string dev_name, void *addr, size_t length);

        bool invalid_cache(buf *buf, bool write_back = false);

        ~cache_inv() noexcept(false);

    private:
        constexpr static uint8_t PORT_NUM = 1;
        constexpr static uint8_t INDEX_NUM = 1;
        // poll cq after BATCH_NUM submit
        constexpr static int BATCH_NUM = 64;
        ibv_device *get_ibv_device(std::string dev_name);

        int submit_index{};

        DOCA::dev *dev;

        ibv_device *ibv_dev;
        ibv_device **device_list;
        ibv_context *ibv_ctx;
        ibv_pd *pd;
        ibv_cq *cq;
        ibv_qp *qp;
        ibv_mr *mr;
    };
}