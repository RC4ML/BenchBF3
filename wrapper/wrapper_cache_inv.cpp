#include "wrapper_cache_inv.h"

DOCA_LOG_REGISTER(WRAPPER_CACHE_INV);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace DOCA {

    ibv_device *cache_inv::get_ibv_device(std::string dev_name) {
        device_list = ibv_get_device_list(nullptr);
        ibv_device *device = nullptr;

        for (device = *device_list; device != NULL; device = *(++device_list)) {
            if (strcmp(dev_name.c_str(), ibv_get_device_name(device)) == 0) {
                break;
            }
        }
        if (device == nullptr) {
            DOCA_LOG_ERR("Device not found %s\n", dev_name.c_str());
            throw DOCAException(DOCA_ERROR_NOT_FOUND);
        }
        return device;
    }

    cache_inv::cache_inv(std::string dev_name, DOCA::dev *dev) {
        ibv_dev = get_ibv_device(dev_name);

        mlx5dv_context_attr dv_attr = {};

        dv_attr.flags |= MLX5DV_CONTEXT_FLAGS_DEVX;
        ibv_ctx = mlx5dv_open_device(ibv_dev, &dv_attr);
        if (ibv_ctx == nullptr) {
            DOCA_LOG_ERR("Failed to create context %s\n", dev_name.c_str());
            exit(1);
        }

        doca_error_t error = doca_rdma_bridge_get_dev_pd(dev->get_inner_ptr(), &pd);
        if (error != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to create pd %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_cq_init_attr_ex cq_attr = {
                .cqe = 256,
                .cq_context = nullptr,
                .channel = nullptr,
                .comp_vector = 0
        };
        ibv_cq_ex *cq_ex = mlx5dv_create_cq(ibv_ctx, &cq_attr, NULL);
        if (cq_ex == nullptr) {
            DOCA_LOG_ERR("Failed to create cq %s\n", dev_name.c_str());
            exit(1);
        }
        cq = ibv_cq_ex_to_cq(cq_ex);

        ibv_qp_cap qp_cap = {
        .max_send_wr = 256,
        .max_recv_wr = 256,
        .max_send_sge = 1,
        .max_recv_sge = 1,
        .max_inline_data = 64
        };
        ibv_qp_init_attr_ex init_attr = {
                .qp_context = NULL,
                .send_cq = cq,
                .recv_cq = cq,
                .cap = qp_cap,
                .qp_type = IBV_QPT_RC,
                .sq_sig_all = 0,
                .comp_mask = IBV_QP_INIT_ATTR_PD | IBV_QP_INIT_ATTR_SEND_OPS_FLAGS,
                .pd = pd,
                .send_ops_flags = IBV_QP_EX_WITH_RDMA_WRITE | IBV_QP_EX_WITH_RDMA_WRITE_WITH_IMM | \
                        IBV_QP_EX_WITH_SEND | IBV_QP_EX_WITH_SEND_WITH_IMM | IBV_QP_EX_WITH_RDMA_READ,
        };
        mlx5dv_qp_init_attr attr_dv = {
                .comp_mask = MLX5DV_QP_INIT_ATTR_MASK_SEND_OPS_FLAGS,
                .send_ops_flags = MLX5DV_QP_EX_WITH_MEMCPY,
        };
        qp = mlx5dv_create_qp(ibv_ctx, &init_attr, &attr_dv);
        if (qp == nullptr) {
            DOCA_LOG_ERR("Failed to create qp %s\n", dev_name.c_str());
            exit(1);
        }

        int mask = IBV_QP_STATE | IBV_QP_PORT | IBV_QP_PKEY_INDEX | IBV_QP_ACCESS_FLAGS;
        ibv_qp_attr attr = {
                .qp_state = IBV_QPS_INIT,
                .qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ,
                .pkey_index = 0,
                .port_num = 1,
        };
        if (ibv_modify_qp(qp, &attr, mask)) {
            DOCA_LOG_ERR("Failed to init qp %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_device_attr dev_attr = {};
        if (ibv_query_device(ibv_ctx, &dev_attr) || dev_attr.phys_port_cnt != PORT_NUM) {
            DOCA_LOG_ERR("Failed to query device info %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_port_attr port_attr = {};
        if (ibv_query_port(ibv_ctx, PORT_NUM, &port_attr) ||
            port_attr.state != IBV_PORT_ACTIVE ||
            port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
            DOCA_LOG_ERR("Failed to query port info %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_gid gid = {};
        if (ibv_query_gid(ibv_ctx, PORT_NUM, INDEX_NUM, &gid)) {
            DOCA_LOG_ERR("Failed to query gid info %s\n", dev_name.c_str());
            exit(1);
        }

        mask = IBV_QP_STATE | IBV_QP_AV | \
            IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | \
            IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
        ibv_qp_attr qpa = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = IBV_MTU_1024,
        .rq_psn = 0,
        .dest_qp_num = qp->qp_num,
        .ah_attr = {
            .grh = {
                .dgid = gid,
                .sgid_index = INDEX_NUM,
                .hop_limit = 64,
            },
            .is_global = 1,
            .port_num = PORT_NUM,
        },
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 0x12,
        };
        if (ibv_modify_qp(qp, &qpa, mask)) {
            DOCA_LOG_ERR("Failed to modity qp to rtr %s\n", dev_name.c_str());
            exit(1);
        }

        qpa.qp_state = IBV_QPS_RTS;
        qpa.timeout = 12;
        qpa.retry_cnt = 6;
        qpa.rnr_retry = 0;
        qpa.sq_psn = 0;
        qpa.max_rd_atomic = 1;
        mask = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | \
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
        if (ibv_modify_qp(qp, &qpa, mask)) {
            DOCA_LOG_ERR("Failed to modity qp to rts %s\n", dev_name.c_str());
            exit(1);
        }
    }

    cache_inv::cache_inv(std::string dev_name, void *addr, size_t length) {
        ibv_dev = get_ibv_device(dev_name);

        mlx5dv_context_attr dv_attr = {};

        dv_attr.flags |= MLX5DV_CONTEXT_FLAGS_DEVX;
        ibv_ctx = mlx5dv_open_device(ibv_dev, &dv_attr);
        if (ibv_ctx == nullptr) {
            DOCA_LOG_ERR("Failed to create context %s\n", dev_name.c_str());
            exit(1);
        }
        // create a new pd
        pd = ibv_alloc_pd(ibv_ctx);

        // create a new mr
        mr = ibv_reg_mr(pd, addr, length, IBV_ACCESS_LOCAL_WRITE | \
            IBV_ACCESS_REMOTE_READ | \
            IBV_ACCESS_REMOTE_WRITE);

        ibv_cq_init_attr_ex cq_attr = {
                .cqe = 256,
                .cq_context = nullptr,
                .channel = nullptr,
                .comp_vector = 0
        };
        ibv_cq_ex *cq_ex = mlx5dv_create_cq(ibv_ctx, &cq_attr, NULL);
        if (cq_ex == nullptr) {
            DOCA_LOG_ERR("Failed to create cq %s\n", dev_name.c_str());
            exit(1);
        }
        cq = ibv_cq_ex_to_cq(cq_ex);

        ibv_qp_cap qp_cap = {
        .max_send_wr = 256,
        .max_recv_wr = 256,
        .max_send_sge = 1,
        .max_recv_sge = 1,
        .max_inline_data = 64
        };
        ibv_qp_init_attr_ex init_attr = {
                .qp_context = NULL,
                .send_cq = cq,
                .recv_cq = cq,
                .cap = qp_cap,
                .qp_type = IBV_QPT_RC,
                .sq_sig_all = 0,
                .comp_mask = IBV_QP_INIT_ATTR_PD | IBV_QP_INIT_ATTR_SEND_OPS_FLAGS,
                .pd = pd,
                .send_ops_flags = IBV_QP_EX_WITH_RDMA_WRITE | IBV_QP_EX_WITH_RDMA_WRITE_WITH_IMM | \
                        IBV_QP_EX_WITH_SEND | IBV_QP_EX_WITH_SEND_WITH_IMM | IBV_QP_EX_WITH_RDMA_READ,
        };
        mlx5dv_qp_init_attr attr_dv = {
                .comp_mask = MLX5DV_QP_INIT_ATTR_MASK_SEND_OPS_FLAGS,
                .send_ops_flags = MLX5DV_QP_EX_WITH_MEMCPY,
        };
        qp = mlx5dv_create_qp(ibv_ctx, &init_attr, &attr_dv);
        if (qp == nullptr) {
            DOCA_LOG_ERR("Failed to create qp %s\n", dev_name.c_str());
            exit(1);
        }

        int mask = IBV_QP_STATE | IBV_QP_PORT | IBV_QP_PKEY_INDEX | IBV_QP_ACCESS_FLAGS;
        ibv_qp_attr attr = {
                .qp_state = IBV_QPS_INIT,
                .qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ,
                .pkey_index = 0,
                .port_num = 1,
        };
        if (ibv_modify_qp(qp, &attr, mask)) {
            DOCA_LOG_ERR("Failed to init qp %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_device_attr dev_attr = {};
        if (ibv_query_device(ibv_ctx, &dev_attr) || dev_attr.phys_port_cnt != PORT_NUM) {
            DOCA_LOG_ERR("Failed to query device info %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_port_attr port_attr = {};
        if (ibv_query_port(ibv_ctx, PORT_NUM, &port_attr) ||
            port_attr.state != IBV_PORT_ACTIVE ||
            port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
            DOCA_LOG_ERR("Failed to query port info %s\n", dev_name.c_str());
            exit(1);
        }

        ibv_gid gid = {};
        if (ibv_query_gid(ibv_ctx, PORT_NUM, INDEX_NUM, &gid)) {
            DOCA_LOG_ERR("Failed to query gid info %s\n", dev_name.c_str());
            exit(1);
        }

        mask = IBV_QP_STATE | IBV_QP_AV | \
            IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | \
            IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
        ibv_qp_attr qpa = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = IBV_MTU_1024,
        .rq_psn = 0,
        .dest_qp_num = qp->qp_num,
        .ah_attr = {
            .grh = {
                .dgid = gid,
                .sgid_index = INDEX_NUM,
                .hop_limit = 64,
            },
            .is_global = 1,
            .port_num = PORT_NUM,
        },
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 0x12,
        };
        if (ibv_modify_qp(qp, &qpa, mask)) {
            DOCA_LOG_ERR("Failed to modity qp to rtr %s\n", dev_name.c_str());
            exit(1);
        }

        qpa.qp_state = IBV_QPS_RTS;
        qpa.timeout = 12;
        qpa.retry_cnt = 6;
        qpa.rnr_retry = 0;
        qpa.sq_psn = 0;
        qpa.max_rd_atomic = 1;
        mask = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | \
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
        if (ibv_modify_qp(qp, &qpa, mask)) {
            DOCA_LOG_ERR("Failed to modity qp to rts %s\n", dev_name.c_str());
            exit(1);
        }
    }

    cache_inv::~cache_inv() noexcept(false) {
        ibv_destroy_qp(qp);
        ibv_destroy_cq(cq);
        if (mr) {
            ibv_dereg_mr(mr);
        }
        // don't need to free PD, because PD belong to doca_dev
        ibv_close_device(ibv_ctx);
        ibv_free_device_list(device_list);
    }

    bool cache_inv::invalid_cache(buf *buf, bool write_back) {
#if defined(__x86_64__)
        (void)buf;
        (void)write_back;
        return false;
#else
        uint32_t mkey;
        if (dev) {
            doca_error_t error = doca_rdma_bridge_get_buf_mkey(buf->get_inner_ptr(), dev->get_inner_ptr(), &mkey);
            if (error != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Failed to invalid cache %p", buf->get_data());
                exit(1);
            }
        } else {
            mkey = mr->lkey;
        }
        uint64_t begin_addr = reinterpret_cast<uint64_t>(buf->get_data());
        uint64_t end_addr = begin_addr + buf->get_length();

        begin_addr = (begin_addr + 63) & ~(63);
        end_addr = end_addr & ~(63);
        if (unlikely(begin_addr == end_addr)) {
            DOCA_LOG_INFO("invalid length < 64B, skip...\n");
            return false;
        }

        ibv_qp_ex *qpx = ibv_qp_to_qp_ex(qp);
        mlx5dv_qp_ex *mqpx = mlx5dv_qp_ex_from_ibv_qp_ex(qpx);

        ibv_wr_start(qpx);
        qpx->wr_id = submit_index++;
        qpx->wr_flags = IBV_SEND_SIGNALED;

        mlx5dv_wr_invcache(mqpx, mkey, begin_addr, end_addr - begin_addr, write_back); // no writeback
        if (ibv_wr_complete(qpx)) {
            DOCA_LOG_ERR("Failed to complete request %d", submit_index);
            exit(1);
        }

        if (submit_index % BATCH_NUM == 0) {
            ibv_wc wc[BATCH_NUM];
            ibv_poll_cq(qp->send_cq, BATCH_NUM, wc);
            // DOCA_LOG_INFO("Poll %d cq\n", count);
            // don't need check just POC
        }
        return true;
#endif
    }

}

#pragma GCC diagnostic pop
