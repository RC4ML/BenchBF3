#pragma once

#include "wrapper_interface.h"
#include "wrapper_buffer.h"
#include "wrapper_device.h"
namespace DOCA {

    using rdma_task_send_completion_cb = doca_rdma_task_send_completion_cb_t;
    using rdma_task_receive_completion_cb = doca_rdma_task_receive_completion_cb_t;
    class rdma: public Base<::doca_rdma>, public engine_to_ctx {
    public:
        explicit rdma(dev *dev, uint32_t gid_index, uint32_t rdma_permissions = DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE | DOCA_ACCESS_FLAG_RDMA_ATOMIC);

        ~rdma() noexcept(false) override;

        doca_error_t set_task_send_conf(uint32_t num_tasks, rdma_task_send_completion_cb completion_cb, rdma_task_send_completion_cb error_cb);

        doca_error_t set_task_receive_conf(uint32_t num_tasks, rdma_task_receive_completion_cb completion_cb, rdma_task_receive_completion_cb error_cb);

        std::string export_rdma();

        void connect_rdma(std::string &exporter);

        inline ::doca_ctx *to_ctx() override {
            return doca_rdma_as_ctx(inner_rdma);
        }

        inline ::doca_rdma *get_inner_ptr() override {
            return inner_rdma;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_rdma *inner_rdma{};
    };

    class rdma_task_send: public Base<::doca_rdma_task_send>, public to_base_job {
    public:
        inline explicit rdma_task_send(rdma *rdma_ptr, buf *src_buf, doca_data task_user_data = {}) {
            doca_error_t result = doca_rdma_task_send_allocate_init(rdma_ptr->get_inner_ptr(), src_buf->get_inner_ptr(), task_user_data, &inner_job);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            src_buff = src_buf;
        }

        inline ~rdma_task_send() noexcept(false) override {
            doca_task_free(to_base());
        }

        inline ::doca_task *to_base() override {
            return doca_rdma_task_send_as_task(inner_job);
        }

        inline ::doca_rdma_task_send *get_inner_ptr() override {
            return inner_job;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_rdma_task_send *inner_job;
        buf *src_buff;
    };

    class rdma_task_receive: public Base<::doca_rdma_task_receive>, public to_base_job {
    public:
        inline explicit rdma_task_receive(rdma *rdma_ptr, buf *src_buf, doca_data task_user_data = {}) {
            doca_error_t result = doca_rdma_task_receive_allocate_init(rdma_ptr->get_inner_ptr(), src_buf->get_inner_ptr(), task_user_data, &inner_job);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            src_buf = src_buf;
        }

        inline ~rdma_task_receive() noexcept(false) override {
            doca_task_free(to_base());
        }

        inline ::doca_task *to_base() override {
            return doca_rdma_task_receive_as_task(inner_job);
        }

        inline ::doca_rdma_task_receive *get_inner_ptr() override {
            return inner_job;
        }

        [[nodiscard]] std::string get_type_name() const override;
    private:
        ::doca_rdma_task_receive *inner_job;
        buf *src_buf;
    };

    doca_error_t rdma_send_job_is_supported(struct doca_devinfo *devinfo);

    doca_error_t rdma_receive_job_is_supported(struct doca_devinfo *devinfo);
}