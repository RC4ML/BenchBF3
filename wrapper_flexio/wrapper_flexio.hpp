#pragma once

#include <string>

#include <infiniband/verbs.h>
#include <infiniband/mlx5_api.h>
#include <libflexio/flexio.h>

#include "common.hpp"
#include "common_cross.h"
#include "hs_clock.h"

#define PRINTF_BUFF_BSIZE (4 * 2048)

/* Convert logarithm to value */
#define L2V(l) (1UL << (l))
/* Convert logarithm to mask */
#define L2M(l) (L2V(l) - 1)

namespace FLEX {
    template <typename A>
    class Base {
    public:
        virtual A *get_inner_ptr() = 0;

        virtual std::string get_type_name() const = 0;

        virtual ~Base() noexcept(false) = default;
    };

    class Context: public Base<::ibv_context> {
    public:
        explicit Context(std::string device_name);

        ~Context() noexcept(false) override;

        ::ibv_context *get_inner_ptr() override;

        std::string get_type_name() const override;

        ::flexio_uar *get_flexio_uar();

        uint32_t get_flexio_uar_id();

        ::ibv_pd *get_pd();

        // ::flexio_prm_hca_caps *get_hca_caps();

        ::flexio_process *get_process();

        ::flexio_event_handler *get_event_handler(size_t index = 0);

        ::flexio_window *get_window();

        uint32_t get_window_id();

        // ::flexio_outbox* get_out_box();

        void alloc_pd();

        void *alloc_huge(size_t size);

        // void query_hca_caps();

        void create_process(flexio_app *app_device);

        // it is usually used after create_process!
        void generate_flexio_uar();

        void create_window();

        ::ibv_mr *get_mr(size_t index);

        ibv_mr *reg_mr(void *addr, size_t size, int flag = IBV_ACCESS_LOCAL_WRITE);

        flexio_mkey *create_mkey(flexio_uintptr_t daddr, size_t size, int access);

        void print_init(const std::string &f_name_prefix, int num_threads);

        void flush();

        // void create_outbox();

        flexio_event_handler *create_event_handler(flexio_func_t func);

        void event_handler_run(flexio_uintptr_t ddata, size_t index = 0);

        void allocate_dbr(flexio_uintptr_t *dbr_daddr);

        flexio_mkey *create_dpa_mkey(flexio_uintptr_t daddr, uint32_t log_buf_size, int flags);

        template <typename T>
        flexio_uintptr_t move_to_dev(const T &arg) {
            flexio_uintptr_t daddr;
            flexio_status ret = flexio_copy_from_host(process, const_cast<T *>(&arg), sizeof(arg), &daddr);
            Assert(ret == FLEXIO_STATUS_SUCCESS);
            return daddr;
        }

    private:
        ::ibv_context *inner_ibv_context{};
        ::ibv_pd *pd{};
        // ::flexio_prm_hca_caps *hca_caps{};
        ::flexio_process *process{};
        ::flexio_uar *flexio_uar{};
        // ::flexio_outbox *outbox{};
        std::vector<::flexio_event_handler *> event_handler_vec{};
        ::flexio_window *window{};
        std::vector<::ibv_mr *> mr_vec{};
        ::flexio_msg_stream *default_stream{};
        ::flexio_msg_stream **streams{};
        int number_of_threads;
    };

    class CommandQ: public Base<::flexio_cmdq> {
    public:
        explicit CommandQ(flexio_process *process, int num_threads, int batch_size);

        ~CommandQ() noexcept(false) override;

        ::flexio_cmdq *get_inner_ptr() override;

        std::string get_type_name() const override;

        void add_task(flexio_func_t func, flexio_uintptr_t ddata);

        void run();

        long wait_run(int time_out_seconds);

        ::flexio_cmdq *cmd_q{};
        DOCA::Timer timer;
    };

    class CQ: public Base<::flexio_cq> {
    public:
        explicit CQ(bool is_rq, uint8_t log_depth, Context *ctx, size_t event_index = 0);

        app_transfer_cq get_cq_transf();

        uint32_t get_cq_num();

        ::flexio_cq *get_inner_ptr() override;

        std::string get_type_name() const override;

    private:
        void allocate_cq_memory(Context *ctx);

        ::flexio_cq *inner_cq_ptr{};
        app_transfer_cq cq_transf{};
        uint8_t inner_log_depth;
    };

    class RQ;

    class SQ: public Base<::flexio_sq> {
    public:
        explicit SQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, bool rqbuff_on_host = false);

        explicit SQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, RQ *rq);

        app_transfer_wq get_sq_transf();

        ::flexio_sq *get_inner_ptr() override;

        std::string get_type_name() const override;

    private:
        void allocate_sq_dpa_memory(Context *ctx);
        ibv_mr *allocate_sq_host_memory(Context *ctx);

        ::flexio_sq *inner_sq_ptr{};
        app_transfer_wq sq_transf{};
        flexio_mkey *sqd_mkey{};
        uint8_t inner_log_depth;
        uint32_t inner_data_size;
    };

    class RQ: public Base<::flexio_rq> {
    public:
        explicit RQ(uint8_t log_depth, uint32_t log_data_size, uint32_t cq_num, Context *ctx, bool rqbuff_on_host = false);

        app_transfer_wq get_rq_transf();

        ::flexio_rq *get_inner_ptr() override;

        std::string get_type_name() const override;

    private:
        void allocate_rq_dpa_memory(Context *ctx);

        ibv_mr *allocate_rq_host_memory(Context *ctx);

        void init_dpa_rq_ring(Context *ctx, uint32_t mkey_id);

        ::flexio_rq *inner_rq_ptr{};
        app_transfer_wq rq_transf{};
        flexio_mkey *rqd_mkey{};
        uint8_t inner_log_depth;
        uint32_t inner_data_size;
        bool rq_on_host;
    };

    struct mlx5_ifc_dr_match_spec_bits {
        uint8_t smac_47_16[0x20];

        uint8_t smac_15_0[0x10];
        uint8_t ethertype[0x10];

        uint8_t dmac_47_16[0x20];

        uint8_t dmac_15_0[0x10];
        uint8_t first_prio[0x3];
        uint8_t first_cfi[0x1];
        uint8_t first_vid[0xc];

        uint8_t ip_protocol[0x8];
        uint8_t ip_dscp[0x6];
        uint8_t ip_ecn[0x2];
        uint8_t cvlan_tag[0x1];
        uint8_t svlan_tag[0x1];
        uint8_t frag[0x1];
        uint8_t ip_version[0x4];
        uint8_t tcp_flags[0x9];

        uint8_t tcp_sport[0x10];
        uint8_t tcp_dport[0x10];

        uint8_t reserved_at_c0[0x18];
        uint8_t ip_ttl_hoplimit[0x8];

        uint8_t udp_sport[0x10];
        uint8_t udp_dport[0x10];

        uint8_t src_ip_127_96[0x20];

        uint8_t src_ip_95_64[0x20];

        uint8_t src_ip_63_32[0x20];

        uint8_t src_ip_31_0[0x20];

        uint8_t dst_ip_127_96[0x20];

        uint8_t dst_ip_95_64[0x20];

        uint8_t dst_ip_63_32[0x20];

        uint8_t dst_ip_31_0[0x20];
    };

    class flow_matcher {
    public:
        static const size_t MATCH_SIZE = 64;

        mlx5dv_flow_match_parameters *to_param();

        void clear();

        void set_src_mac_mask();

        void set_src_mac(size_t mac);

        void set_dst_mac_mask();

        void set_dst_mac(size_t mac);

        void set_src_ipv4_mask();

        void set_src_ipv4(size_t ip);

        void set_dst_ipv4_mask();

        void set_dst_ipv4(size_t ip);

        void set_src_udp_port_mask();

        void set_src_udp_port(uint16_t port);

        void set_dst_udp_port_mask();

        void set_dst_udp_port(uint16_t port);

    private:
        size_t match_size = MATCH_SIZE;
        uint64_t match_buf[MATCH_SIZE];
    };

    class dr_flow_table {
    public:
        mlx5dv_dr_table *dr_table;
        mlx5dv_dr_matcher *dr_matcher;
    };

    class dr_flow_rule {
    public:
        // used for rx action
        void add_dest_devx_tir(flexio_rq *rq_ptr);

        // used for tx root action
        void add_dest_table(mlx5dv_dr_table *dr_table);

        // used for tx action, vport be set to 0xFFFF
        void add_dest_vport(mlx5dv_dr_domain *dr_domain, uint32_t vport);

        void create_dr_rule(dr_flow_table *table, flow_matcher *matcher);

        std::vector<mlx5dv_dr_action *> dr_actions;
        mlx5dv_dr_rule *dr_rule;
    };

    class DR: public Base<::mlx5dv_dr_domain> {
    public:
        explicit DR(Context *ctx, mlx5dv_dr_domain_type type);

        dr_flow_table *create_flow_table(int level, int priority, flow_matcher *matcher);

        ::mlx5dv_dr_domain *get_inner_ptr() override;

        std::string get_type_name() const override;

    private:
        ::mlx5dv_dr_domain *inner_domain;
        mlx5dv_dr_domain_type inner_type;
    };
}