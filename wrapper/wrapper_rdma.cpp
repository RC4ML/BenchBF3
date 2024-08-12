#include "wrapper_rdma.h"

DOCA_LOG_REGISTER(WRAPPER_RDMA);

namespace DOCA {
    rdma::rdma(dev *dev, uint32_t gid_index, uint32_t rdma_permissions) {
        doca_error_t result = doca_rdma_create(dev->get_inner_ptr(), &inner_rdma);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        result = doca_rdma_set_permissions(inner_rdma, rdma_permissions);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        result = doca_rdma_set_gid_index(inner_rdma, gid_index);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    rdma::~rdma() noexcept(false) {
        doca_error_t result = doca_rdma_destroy(inner_rdma);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("rdma destroyed");
    }

    doca_error_t rdma::set_task_send_conf(uint32_t num_tasks, rdma_task_send_completion_cb completion_cb, rdma_task_send_completion_cb error_cb) {
        return doca_rdma_task_send_set_conf(inner_rdma, completion_cb, error_cb, num_tasks);
    }

    doca_error_t rdma::set_task_receive_conf(uint32_t num_tasks, rdma_task_receive_completion_cb completion_cb, rdma_task_receive_completion_cb error_cb) {
        return doca_rdma_task_receive_set_conf(inner_rdma, completion_cb, error_cb, num_tasks);
    }

    std::string rdma::export_rdma() {
        const void *export_desc;
        size_t export_desc_len;
        doca_error_t error = doca_rdma_export(inner_rdma, &export_desc, &export_desc_len);
        if (error != DOCA_SUCCESS) {
            throw DOCAException(error, __FILE__, __LINE__);
        }
        return std::string(reinterpret_cast<const char *>(export_desc), export_desc_len);
    }

    void rdma::connect_rdma(std::string &exporter) {
        doca_error_t result = doca_rdma_connect(inner_rdma, exporter.c_str(), exporter.size());
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    std::string rdma::get_type_name() const {
        return "doca_rdma";
    }

    std::string rdma_task_send::get_type_name() const {
        return "doca_rdma_task_send";
    }

    std::string rdma_task_receive::get_type_name() const {
        return "doca_rdma_task_receive";
    }


    doca_error_t rdma_send_job_is_supported(struct doca_devinfo *devinfo) {
        return doca_rdma_cap_task_send_is_supported(devinfo);
    }

    doca_error_t rdma_receive_job_is_supported(struct doca_devinfo *devinfo) {
        return doca_rdma_cap_task_receive_is_supported(devinfo);
    }

}