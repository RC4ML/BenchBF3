#pragma once

#include "wrapper_interface.h"
#include "wrapper_ctx.h"
namespace DOCA {
    class pe: public Base<::doca_pe> {

    public:
        explicit pe();

        ~pe() noexcept(false) override;

        void connect_ctx(ctx *ctx);

        void start_ctx();

        inline doca_error_t submit(to_base_job *job) {
            return doca_task_submit(job->to_base());
        }

        inline uint8_t poll_progress() {
            return doca_pe_progress(inner_pe);
        }

        inline size_t get_num_inflight() {
            doca_error_t result;
            size_t num_inflight;
            result = doca_pe_get_num_inflight_tasks(inner_pe, &num_inflight);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return num_inflight;
        }

        inline ::doca_pe *get_inner_ptr() override {
            return inner_pe;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        std::vector<ctx *>inner_ctxs;
        ::doca_pe *inner_pe{};
    };
}