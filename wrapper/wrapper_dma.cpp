#include "wrapper_dma.h"

DOCA_LOG_REGISTER(WRAPPER_DMA);

namespace DOCA {

    dma::dma(dev *dev) {
        doca_error_t result = doca_dma_create(dev->get_inner_ptr(), &inner_dma);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    dma::~dma() noexcept(false) {
        doca_error_t result = doca_dma_destroy(inner_dma);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("dma destroyed");
    }

    doca_error_t dma::set_task_conf(uint32_t num_tasks, dma_task_memcpy_completion_cb completion_cb, dma_task_memcpy_completion_cb error_cb) {
        return doca_dma_task_memcpy_set_conf(inner_dma, completion_cb, error_cb, num_tasks);
    }

    std::string dma::get_type_name() const {
        return "doca_dma";
    }

    std::string dma_task_memcpy::get_type_name() const {
        return "doca_dma_task_memcpy";
    }

    doca_error_t dma_jobs_is_supported(struct doca_devinfo *devinfo) {
        return doca_dma_cap_task_memcpy_is_supported(devinfo);
    }
}