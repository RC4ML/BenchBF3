#include "wrapper_flexio.hpp"
#include "numautil.h"

DOCA_LOG_REGISTER(WRAPPER::FLEXIO);

namespace FLEX {
    Context::Context(std::string device_name) {
        struct ibv_device **dev_list{};
        struct ibv_device *dev{};

        dev_list = ibv_get_device_list(nullptr);
        Assert(dev_list != nullptr);

        for (int i = 0; dev_list[i] != nullptr; i++) {
            if (strcmp(dev_list[i]->name, device_name.c_str()) == 0) {
                dev = dev_list[i];
                break;
            }
        }
        if (dev == nullptr) {
            LOG_E("Device name %s not found, avaliable devices:\n", device_name.c_str());
            for (int i = 0; dev_list[i] != nullptr; i++) {
                printf("%s\n", dev_list[i]->name);
            }
            exit(1);
        }
        Assert(dev != nullptr);

        inner_ibv_context = ibv_open_device(dev);
        Assert(inner_ibv_context != nullptr);
        ibv_free_device_list(dev_list);
    }

    Context::~Context() noexcept(false) {
        int result = ibv_close_device(inner_ibv_context);
        Assert(result == 0);
    }

    ::ibv_context *Context::get_inner_ptr() {
        return inner_ibv_context;
    }

    std::string Context::get_type_name() const {
        return "ibv_context";
    }


    ::flexio_uar *Context::get_flexio_uar() {
        return flexio_uar;
    }

    uint32_t Context::get_flexio_uar_id() {
        Assert(flexio_uar != nullptr);
        return flexio_uar_get_id(flexio_uar);
    }

    ::ibv_pd *Context::get_pd() {
        return pd;
    }

    // ::flexio_prm_hca_caps* Context::get_hca_caps(){
    // 	return hca_caps;
    // }

    ::flexio_process *Context::get_process() {
        return process;
    }

    ::flexio_event_handler *Context::get_event_handler(size_t index) {
        Assert(index < event_handler_vec.size());
        return event_handler_vec[index];
    }

    ::flexio_window *Context::get_window() {
        Assert(window != nullptr);
        return window;
    }

    uint32_t Context::get_window_id() {
        Assert(window != nullptr);
        uint32_t window_id = flexio_window_get_id(window);
        return window_id;
    }

    void Context::alloc_pd() {
        pd = ibv_alloc_pd(inner_ibv_context);
        Assert(pd != nullptr);
    }

    void *Context::alloc_huge(size_t size) {
        uint32_t numa_node = 0;
        void *addr = DOCA::get_huge_mem(numa_node, size); // would break if failed inside
        return addr;
    }

    void Context::create_process(flexio_app *app_device) {
        flexio_process_attr process_attr = { nullptr, 0 };
        flexio_status ret = flexio_process_create(inner_ibv_context, app_device, &process_attr, &process);
        Assert(ret == FLEXIO_STATUS_SUCCESS)
    }

    void Context::generate_flexio_uar() {
        Assert(flexio_uar == nullptr);
        flexio_uar = flexio_process_get_uar(process);
        Assert(flexio_uar != nullptr);
    }

    void Context::create_window() {
        flexio_status ret = flexio_window_create(process, pd, &window);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
    }

    ::ibv_mr *Context::get_mr(size_t index) {
        Assert(index < mr_vec.size());
        return mr_vec[index];
    }

    ibv_mr *Context::reg_mr(void *addr, size_t size, int flag) {
        /*Must be 64B aligned buffer, posix_memalign(&buf, 64, size);*/
        ibv_mr *mr = ibv_reg_mr(pd, addr, size, flag);
        Assert(mr != nullptr);
        mr_vec.push_back(mr);
        return mr;
    }

    flexio_mkey *Context::create_mkey(flexio_uintptr_t daddr, size_t size, int access) {
        flexio_mkey_attr mkey_attr{ pd, daddr, size, access };
        flexio_mkey *mkey;
        Assert(flexio_device_mkey_create(process, &mkey_attr, &mkey) == FLEXIO_STATUS_SUCCESS);
        return mkey;
    }

    void Context::print_init(const std::string &f_name_prefix, int num_threads) {
        flexio_msg_stream_attr_t attr;
        attr.uar = flexio_uar;
        attr.data_bsize = PRINTF_BUFF_BSIZE;
        attr.sync_mode = FLEXIO_LOG_DEV_SYNC_MODE_SYNC;
        attr.level = FLEXIO_MSG_DEV_DEBUG;
        attr.stream_name = const_cast<char *>("Default Stream");
        attr.mgmt_affinity.type = FLEXIO_AFFINITY_NONE;

        flexio_status ret = flexio_msg_stream_create(process, &attr, stdout, nullptr, &default_stream);
        Assert(ret == FLEXIO_STATUS_SUCCESS);

        std::string prefix = "./log/" + f_name_prefix + "_";
        streams = static_cast<flexio_msg_stream **>(malloc(num_threads * sizeof(flexio_msg_stream *)));
        number_of_threads = num_threads;
        for (int i = 0; i < num_threads; i++) {
            std::string file_name = prefix + std::to_string(i) + ".txt";
            attr.stream_name = const_cast<char *>((f_name_prefix + "_" + std::to_string(i)).c_str());
            FILE *fp = fopen(file_name.c_str(), "w");
            Assert(fp != nullptr);
            ret = flexio_msg_stream_create(process, &attr, fp, NULL, &streams[i]);
            Assert(ret == FLEXIO_STATUS_SUCCESS);

            int id = flexio_msg_stream_get_id(streams[i]);
            Assert(id == i + 1);
        }
    }

    void Context::flush() {
        flexio_msg_stream_flush(default_stream);
        for (int i = 0; i < number_of_threads; i++) {
            flexio_msg_stream_flush(streams[i]);
        }
    }

    flexio_event_handler *Context::create_event_handler(flexio_func_t func) {
        struct flexio_event_handler_attr handler_attr {
        };
        struct flexio_event_handler *event_handler;
        handler_attr.host_stub_func = func;
        handler_attr.affinity.type = FLEXIO_AFFINITY_STRICT;
        handler_attr.affinity.id = static_cast<uint32_t>(event_handler_vec.size());
        flexio_status ret = flexio_event_handler_create(process, &handler_attr, &event_handler);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
        event_handler_vec.push_back(event_handler);
        return event_handler;
    }

    void Context::event_handler_run(flexio_uintptr_t ddata, size_t index) {
        Assert(index < event_handler_vec.size());
        flexio_status ret = flexio_event_handler_run(event_handler_vec[index], ddata);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
    }

    void Context::allocate_dbr(flexio_uintptr_t *dbr_daddr) {
        __be32 dbr[2] = { 0, 0 };
        Assert(flexio_copy_from_host(process, dbr, sizeof(dbr), dbr_daddr) == FLEXIO_STATUS_SUCCESS);
    }

    flexio_mkey *Context::create_dpa_mkey(flexio_uintptr_t daddr, uint32_t log_buf_size, int flags) {
        flexio_mkey_attr mkey_attr{};
        mkey_attr.pd = pd;
        mkey_attr.daddr = daddr;
        mkey_attr.len = LOG2VALUE(log_buf_size);
        mkey_attr.access = flags;

        flexio_mkey *res = nullptr;
        Assert(flexio_device_mkey_create(process, &mkey_attr, &res) == FLEXIO_STATUS_SUCCESS);
        return res;
    }

}

namespace FLEX {
    CommandQ::CommandQ(flexio_process *process, int num_threads, int batch_size) {
        flexio_cmdq_attr cmdq_fattr;
        cmdq_fattr.workers = num_threads;
        cmdq_fattr.batch_size = batch_size;
        cmdq_fattr.state = FLEXIO_CMDQ_STATE_PENDING;

        flexio_status ret = flexio_cmdq_create(process, &cmdq_fattr, &cmd_q);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
    }

    CommandQ::~CommandQ() noexcept(false) {
    }

    ::flexio_cmdq *CommandQ::get_inner_ptr() {
        return cmd_q;
    }

    std::string CommandQ::get_type_name() const {
        return "flexio_cmdq";
    }

    void CommandQ::add_task(flexio_func_t func, flexio_uintptr_t ddata) {
        flexio_status ret = flexio_cmdq_task_add(cmd_q, func, ddata);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
    }

    void CommandQ::run() {
        timer.tic();
        flexio_status ret = flexio_cmdq_state_running(cmd_q);
        Assert(ret == FLEXIO_STATUS_SUCCESS);
    }

    long CommandQ::wait_run(int time_out_seconds) { // return us
        int useconds = 0;
        int step_us = 1000;
        while (!flexio_cmdq_is_empty(cmd_q)) {
            usleep(step_us);
            useconds += step_us;
            Assert(useconds < time_out_seconds * 1e6);
        }
        auto duration = timer.toc();
        return duration;
    }

    CQ::CQ(bool is_rq, uint8_t log_depth, Context *ctx, size_t event_index) {
        inner_log_depth = log_depth;
        if (is_rq) {
            flexio_cq_attr rqcq_attr{};
            rqcq_attr.log_cq_depth = log_depth;
            rqcq_attr.element_type = FLEXIO_CQ_ELEMENT_TYPE_DPA_THREAD;
            rqcq_attr.thread = flexio_event_handler_get_thread(ctx->get_event_handler(event_index));
            rqcq_attr.uar_id = ctx->get_flexio_uar_id();
            rqcq_attr.uar_base_addr = 0;

            allocate_cq_memory(ctx);
            rqcq_attr.cq_dbr_daddr = cq_transf.cq_dbr_daddr;
            rqcq_attr.cq_ring_qmem.daddr = cq_transf.cq_ring_daddr;
            Assert(flexio_cq_create(ctx->get_process(), nullptr, &rqcq_attr, &inner_cq_ptr) == FLEXIO_STATUS_SUCCESS);
        } else {
            flexio_cq_attr sqcq_attr{};
            sqcq_attr.log_cq_depth = log_depth;
            sqcq_attr.element_type = FLEXIO_CQ_ELEMENT_TYPE_NON_DPA_CQ;
            sqcq_attr.uar_id = ctx->get_flexio_uar_id();
            sqcq_attr.uar_base_addr = 0;

            allocate_cq_memory(ctx);
            sqcq_attr.cq_dbr_daddr = cq_transf.cq_dbr_daddr;
            sqcq_attr.cq_ring_qmem.daddr = cq_transf.cq_ring_daddr;
            // I think this ibv_ctx can be nullptr, but sample code use it, so let us use it.
            Assert(flexio_cq_create(ctx->get_process(), ctx->get_inner_ptr(), &sqcq_attr, &inner_cq_ptr) ==
                FLEXIO_STATUS_SUCCESS);
        }
        cq_transf.cq_num = get_cq_num();
        cq_transf.log_cq_depth = log_depth;
    }

    app_transfer_cq CQ::get_cq_transf() {
        return cq_transf;
    }

    uint32_t CQ::get_cq_num() {
        return flexio_cq_get_cq_num(inner_cq_ptr);
    }

    ::flexio_cq *CQ::get_inner_ptr() {
        return inner_cq_ptr;
    }

    std::string CQ::get_type_name() const {
        return "flexio_cq";
    }

    void CQ::allocate_cq_memory(Context *ctx) {
        mlx5_cqe64 *cq_ring_src, *cqe;
        size_t ring_bsize;
        int i, num_of_cqes;
        const int log_cqe_bsize = 6; /* CQE size is 64 bytes */

        ctx->allocate_dbr(&cq_transf.cq_dbr_daddr);

        num_of_cqes = LOG2VALUE(inner_log_depth);
        ring_bsize = num_of_cqes * LOG2VALUE(log_cqe_bsize);

        cq_ring_src = static_cast<mlx5_cqe64 *>(calloc(num_of_cqes, LOG2VALUE(log_cqe_bsize)));
        Assert(cq_ring_src);
        cqe = cq_ring_src;
        for (i = 0; i < num_of_cqes; i++) {
            mlx5dv_set_cqe_owner(cqe++, 1);
        }
        Assert(flexio_copy_from_host(ctx->get_process(), cq_ring_src, ring_bsize, &cq_transf.cq_ring_daddr) ==
            FLEXIO_STATUS_SUCCESS);
        free(cq_ring_src);
    }

    // this log_data_size doesn't contain depth size
    SQ::SQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, bool rqbuff_on_host) {
        inner_log_depth = log_depth;
        inner_data_size = log_data_size;
        flexio_wq_attr sq_attr{};
        sq_attr.log_wq_depth = log_depth;
        sq_attr.uar_id = ctx->get_flexio_uar_id();
        sq_attr.pd = ctx->get_pd();
        // IMPORTANT! DON'T KNOW WHAT HAPPEND
        sq_attr.sq.allow_multi_pkt_send_wqe = 1;
        if (rqbuff_on_host) {
            ibv_mr *mr = allocate_sq_host_memory(ctx);
            sq_transf.wqd_mkey_id = mr->lkey;
        } else {
            allocate_sq_dpa_memory(ctx);
            sqd_mkey = ctx->create_dpa_mkey(sq_transf.wqd_daddr, log_depth + log_data_size, IBV_ACCESS_LOCAL_WRITE);
            sq_transf.wqd_mkey_id = flexio_mkey_get_id(sqd_mkey);
        }
        sq_attr.wq_ring_qmem.daddr = sq_transf.wq_ring_daddr;

        Assert(flexio_sq_create(ctx->get_process(), nullptr, cq_num, &sq_attr, &inner_sq_ptr) == FLEXIO_STATUS_SUCCESS);
        sq_transf.wq_num = flexio_sq_get_wq_num(inner_sq_ptr);

        sq_transf.daddr_on_host = rqbuff_on_host;
    }

    SQ::SQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, RQ *rq) {
        inner_log_depth = log_depth;
        inner_data_size = log_data_size;
        flexio_wq_attr sq_attr{};
        sq_attr.log_wq_depth = log_depth;
        sq_attr.uar_id = ctx->get_flexio_uar_id();
        sq_attr.pd = ctx->get_pd();
        // IMPORTANT! DON'T KNOW WHAT HAPPEND
        sq_attr.sq.allow_multi_pkt_send_wqe = 1;


        Assert(rq->get_rq_transf().wqd_daddr != 0 && rq->get_rq_transf().wqd_mkey_id != UINT32_MAX)
            sq_transf.wqd_daddr = rq->get_rq_transf().wqd_daddr;
        const int log_wqe_bsize = 6; /* WQE size is 64 bytes */
        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth + log_wqe_bsize),
            &sq_transf.wq_ring_daddr) == FLEXIO_STATUS_SUCCESS);

        ctx->allocate_dbr(&sq_transf.wq_dbr_daddr);
        sq_attr.wq_ring_qmem.daddr = sq_transf.wq_ring_daddr;
        Assert(flexio_sq_create(ctx->get_process(), nullptr, cq_num, &sq_attr, &inner_sq_ptr) == FLEXIO_STATUS_SUCCESS);
        sq_transf.wq_num = flexio_sq_get_wq_num(inner_sq_ptr);
        sq_transf.wqd_mkey_id = rq->get_rq_transf().wqd_mkey_id;

        sq_transf.daddr_on_host = rq->get_rq_transf().daddr_on_host;
    }

    app_transfer_wq SQ::get_sq_transf() {
        return sq_transf;
    }

    ::flexio_sq *SQ::get_inner_ptr() {
        return inner_sq_ptr;
    }

    std::string SQ::get_type_name() const {
        return "flexio_sq";
    }

    void SQ::allocate_sq_dpa_memory(Context *ctx) {
        const int log_wqe_bsize = 6; /* WQE size is 64 bytes */
        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth + inner_data_size),
            &sq_transf.wqd_daddr) == FLEXIO_STATUS_SUCCESS);

        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth + log_wqe_bsize),
            &sq_transf.wq_ring_daddr) == FLEXIO_STATUS_SUCCESS);

        ctx->allocate_dbr(&sq_transf.wq_dbr_daddr);
    }
    ibv_mr *SQ::allocate_sq_host_memory(Context *ctx) {
        const int log_wqe_bsize = 6; /* WQE size is 64 bytes */

        ibv_mr *res = nullptr;
        void *tmp_ptr = nullptr;
        tmp_ptr = DOCA::get_huge_mem(0, (LOG2VALUE(inner_log_depth + inner_data_size) + 63) & (~63));
        Assert(tmp_ptr != nullptr);
        memset(tmp_ptr, 0, LOG2VALUE(inner_log_depth + inner_data_size));
        res = ctx->reg_mr(tmp_ptr, LOG2VALUE(inner_log_depth + inner_data_size), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
        sq_transf.wqd_daddr = reinterpret_cast<flexio_uintptr_t>(tmp_ptr);

        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth + log_wqe_bsize),
            &sq_transf.wq_ring_daddr) == FLEXIO_STATUS_SUCCESS);

        ctx->allocate_dbr(&sq_transf.wq_dbr_daddr);

        return res;
    }


    RQ::RQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, bool rqbuff_on_host) {
        inner_log_depth = log_depth;
        inner_data_size = log_data_size;
        rq_on_host = rqbuff_on_host;
        flexio_wq_attr rq_attr{};
        rq_attr.log_wq_depth = log_depth;
        rq_attr.pd = ctx->get_pd();

        uint32_t mkey_id = 0;
        if (rqbuff_on_host) {
            ibv_mr *mr = allocate_rq_host_memory(ctx);
            mkey_id = mr->lkey;
        } else {
            allocate_rq_dpa_memory(ctx);
            rqd_mkey = ctx->create_dpa_mkey(rq_transf.wqd_daddr, log_depth + log_data_size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
            mkey_id = flexio_mkey_get_id(rqd_mkey);
        }
        rq_transf.daddr_on_host = rqbuff_on_host;

        init_dpa_rq_ring(ctx, mkey_id);

        rq_attr.wq_dbr_qmem.memtype = FLEXIO_MEMTYPE_DPA;
        rq_attr.wq_dbr_qmem.daddr = rq_transf.wq_dbr_daddr;
        rq_attr.wq_ring_qmem.daddr = rq_transf.wq_ring_daddr;
        Assert(flexio_rq_create(ctx->get_process(), nullptr, cq_num, &rq_attr, &inner_rq_ptr) == FLEXIO_STATUS_SUCCESS);

        rq_transf.wqd_mkey_id = mkey_id;
        rq_transf.wq_num = flexio_rq_get_wq_num(inner_rq_ptr);

        __be32 dbr[2];
        uint32_t rcv_counter = LOG2VALUE(inner_log_depth);
        uint32_t send_counter = 0;
        dbr[0] = htobe32(rcv_counter & 0xffff);
        dbr[1] = htobe32(send_counter & 0xffff);
        Assert(flexio_host2dev_memcpy(ctx->get_process(), dbr, sizeof(dbr), rq_transf.wq_dbr_daddr) ==
            FLEXIO_STATUS_SUCCESS);
    }

    app_transfer_wq RQ::get_rq_transf() {
        return rq_transf;
    }

    ::flexio_rq *RQ::get_inner_ptr() {
        return inner_rq_ptr;
    }

    std::string RQ::get_type_name() const {
        return "flexio_rq";
    }

    void RQ::allocate_rq_dpa_memory(Context *ctx) {
        const int ring_elem_size = sizeof(mlx5_wqe_data_seg); /* WQE size is 64 bytes */

        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth + inner_data_size),
            &rq_transf.wqd_daddr) == FLEXIO_STATUS_SUCCESS);

        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth) * ring_elem_size,
            &rq_transf.wq_ring_daddr) == FLEXIO_STATUS_SUCCESS);

        ctx->allocate_dbr(&rq_transf.wq_dbr_daddr);
    }

    ibv_mr *RQ::allocate_rq_host_memory(Context *ctx) {
        const int ring_elem_size = sizeof(mlx5_wqe_data_seg); /* WQE size is 64 bytes */
        ibv_mr *res = nullptr;

        void *tmp_ptr = nullptr;
        // must on numa 0 and 64B aligned
        tmp_ptr = DOCA::get_huge_mem(0, (LOG2VALUE(inner_log_depth + inner_data_size) + 63) & (~63));
        Assert(tmp_ptr != nullptr);

        memset(tmp_ptr, 0, LOG2VALUE(inner_log_depth + inner_data_size));
        res = ctx->reg_mr(tmp_ptr, LOG2VALUE(inner_log_depth + inner_data_size), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
        rq_transf.wqd_daddr = reinterpret_cast<flexio_uintptr_t>(tmp_ptr);

        Assert(flexio_buf_dev_alloc(ctx->get_process(), LOG2VALUE(inner_log_depth) * ring_elem_size,
            &rq_transf.wq_ring_daddr) == FLEXIO_STATUS_SUCCESS);

        ctx->allocate_dbr(&rq_transf.wq_dbr_daddr);

        return res;
    }

    void RQ::init_dpa_rq_ring(Context *ctx, uint32_t mkey_id) {
        mlx5_wqe_data_seg *rx_wqes, *dseg;
        size_t data_chunk_bsize;
        size_t ring_bsize;
        int num_of_wqes;

        num_of_wqes = LOG2VALUE(inner_log_depth);
        ring_bsize = num_of_wqes * sizeof(mlx5_wqe_data_seg);
        data_chunk_bsize = LOG2VALUE(inner_data_size);

        rx_wqes = static_cast<mlx5_wqe_data_seg *>(calloc(num_of_wqes, sizeof(mlx5_wqe_data_seg)));
        dseg = rx_wqes;

        flexio_uintptr_t wqd_daddr = rq_transf.wqd_daddr;
        for (int i = 0; i < num_of_wqes; i++) {
            mlx5dv_set_data_seg(dseg, data_chunk_bsize, mkey_id, wqd_daddr);
            dseg++;
            wqd_daddr += data_chunk_bsize;
        }

        Assert(flexio_host2dev_memcpy(ctx->get_process(), rx_wqes, ring_bsize, rq_transf.wq_ring_daddr) ==
            FLEXIO_STATUS_SUCCESS);
        free(rx_wqes);
    }

    mlx5dv_flow_match_parameters *flow_matcher::to_param() {
        return reinterpret_cast<mlx5dv_flow_match_parameters *>(this);
    }

    void flow_matcher::clear() {
        memset(match_buf, 0, sizeof(uint64_t) * MATCH_SIZE);
    }

    void flow_matcher::set_src_mac_mask() {
        DEVX_SET(dr_match_spec, match_buf, smac_47_16, 0xffffffff);
        DEVX_SET(dr_match_spec, match_buf, smac_15_0, 0xffff);
    }

    void flow_matcher::set_src_mac(size_t mac) {
        DEVX_SET(dr_match_spec, match_buf, smac_47_16, mac >> 16);
        DEVX_SET(dr_match_spec, match_buf, smac_15_0, mac % (1 << 16));
    }

    void flow_matcher::set_dst_mac_mask() {
        DEVX_SET(dr_match_spec, match_buf, dmac_47_16, 0xffffffff);
        DEVX_SET(dr_match_spec, match_buf, dmac_15_0, 0xffff);
    }

    void flow_matcher::set_dst_mac(size_t mac) {
        DEVX_SET(dr_match_spec, match_buf, dmac_47_16, mac >> 16);
        DEVX_SET(dr_match_spec, match_buf, dmac_15_0, mac % (1 << 16));
    }

    void flow_matcher::set_src_ipv4_mask() {
        DEVX_SET(dr_match_spec, match_buf, src_ip_31_0, 0xffffffff);
    }

    void flow_matcher::set_src_ipv4(size_t ip) {
        DEVX_SET(dr_match_spec, match_buf, src_ip_31_0, ip);
    }

    void flow_matcher::set_dst_ipv4_mask() {
        DEVX_SET(dr_match_spec, match_buf, dst_ip_31_0, 0xffffffff);
    }

    void flow_matcher::set_dst_ipv4(size_t ip) {
        DEVX_SET(dr_match_spec, match_buf, dst_ip_31_0, ip);
    }

    void flow_matcher::set_src_udp_port_mask() {
        DEVX_SET(dr_match_spec, match_buf, udp_sport, 0xffff);
    }

    void flow_matcher::set_src_udp_port(uint16_t port) {
        DEVX_SET(dr_match_spec, match_buf, udp_sport, port);
    }

    void flow_matcher::set_dst_udp_port_mask() {
        DEVX_SET(dr_match_spec, match_buf, udp_dport, 0xffff);
    }

    void flow_matcher::set_dst_udp_port(uint16_t port) {
        DEVX_SET(dr_match_spec, match_buf, udp_dport, port);
    }

    void dr_flow_rule::add_dest_devx_tir(flexio_rq *rq_ptr) {
        mlx5dv_dr_action *action = mlx5dv_dr_action_create_dest_devx_tir(flexio_rq_get_tir(rq_ptr));
        Assert(action != nullptr);
        dr_actions.push_back(action);
    }

    void dr_flow_rule::add_dest_table(mlx5dv_dr_table *dr_table) {
        mlx5dv_dr_action *action = mlx5dv_dr_action_create_dest_table(dr_table);
        Assert(action != nullptr);
        dr_actions.push_back(action);
    }

    void dr_flow_rule::add_dest_vport(mlx5dv_dr_domain *dr_domain, uint32_t vport) {
        mlx5dv_dr_action *action = mlx5dv_dr_action_create_dest_vport(dr_domain, vport);
        Assert(action != nullptr);
        dr_actions.push_back(action);
    }

    void dr_flow_rule::create_dr_rule(dr_flow_table *table, flow_matcher *matcher) {
        Assert(!dr_actions.empty());
        dr_rule = mlx5dv_dr_rule_create(table->dr_matcher, matcher->to_param(), dr_actions.size(), dr_actions.data());
        Assert(dr_rule);
    }

    DR::DR(Context *ctx, mlx5dv_dr_domain_type type) {
        inner_type = type;
        inner_domain = mlx5dv_dr_domain_create(ctx->get_inner_ptr(), type);
    }

    dr_flow_table *DR::create_flow_table(int level, int priority, flow_matcher *matcher) {
        uint8_t criteria_enable = 0x1; /* Criteria enabled  */

        auto *table = new dr_flow_table();
        table->dr_table = mlx5dv_dr_table_create(inner_domain, level);
        table->dr_matcher = mlx5dv_dr_matcher_create(table->dr_table, priority, criteria_enable, matcher->to_param());

        return table;
    }

    ::mlx5dv_dr_domain *DR::get_inner_ptr() {
        return inner_domain;
    }

    std::string DR::get_type_name() const {
        return "mlx5dv_dr_domain";
    }

}