#pragma once

#include "wrapper_interface.h"
#include "wrapper_buffer.h"
#include "wrapper_device.h"
namespace DOCA {

    using dma_task_memcpy_completion_cb = doca_dma_task_memcpy_completion_cb_t;

    class dma: public Base<::doca_dma>, public engine_to_ctx {
    public:
        explicit dma(dev *dev);

        ~dma() noexcept(false) override;

        doca_error_t set_task_conf(uint32_t num_tasks, dma_task_memcpy_completion_cb completion_cb, dma_task_memcpy_completion_cb error_cb);

        inline ::doca_ctx *to_ctx() override {
            return doca_dma_as_ctx(inner_dma);
        }

        inline ::doca_dma *get_inner_ptr() override {
            return inner_dma;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_dma *inner_dma{};
    };

    class dma_task_memcpy: public Base<::doca_dma_task_memcpy>, public to_base_job {
    public:
        inline explicit dma_task_memcpy(dma *dma_ptr, buf *src_buf, buf *dst_buf, doca_data task_user_data = {}) {
            doca_error_t result = doca_dma_task_memcpy_alloc_init(dma_ptr->get_inner_ptr(), src_buf->get_inner_ptr(), dst_buf->get_inner_ptr(), task_user_data, &inner_job);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            src_buff = src_buf;
            dst_buff = dst_buf;
        }

        inline ~dma_task_memcpy() noexcept(false) override {
            doca_task_free(to_base());
        }

        inline void set_src(buf *src) {
            src_buff = src;
            doca_dma_task_memcpy_set_src(inner_job, src->get_inner_ptr());
        }

        inline void set_dst(buf *dst) {
            dst_buff = dst;
            doca_dma_task_memcpy_set_dst(inner_job, dst->get_inner_ptr());
        }

        inline buf *get_src() {
            return src_buff;
        }

        inline buf *get_dst() {
            return dst_buff;
        }

        inline ::doca_task *to_base() override {
            return doca_dma_task_memcpy_as_task(inner_job);
        }

        inline ::doca_dma_task_memcpy *get_inner_ptr() override {
            return inner_job;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_dma_task_memcpy *inner_job;
        buf *src_buff;
        buf *dst_buff;
    };

    doca_error_t dma_jobs_is_supported(struct doca_devinfo *devinfo);
}