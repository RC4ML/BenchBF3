#include "wrapper_pcc.h"

DOCA_LOG_REGISTER("WRAPPER_PCC");

namespace DOCA {
    pcc::pcc(dev *dev) {
        doca_error_t result = doca_pcc_create(dev->get_inner_ptr(), &inner_pcc);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    pcc::~pcc() noexcept(false) {
        doca_error_t result = doca_pcc_destroy(inner_pcc);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    // return min_num_threads and max_num_threads
    std::pair<uint32_t, uint32_t> pcc::get_num_threads() {
        uint32_t threads[2];

        doca_error_t result = doca_pcc_get_min_num_threads(inner_pcc, &threads[0]);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }

        result = doca_pcc_get_max_num_threads(inner_pcc, &threads[1]);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }

        return {threads[0], threads[1]};
    }

    void pcc::set_app(doca_pcc_app *app) {
        doca_error_t result = doca_pcc_set_app(inner_pcc, app);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    void pcc::set_thread_affinity(uint32_t num_threads, uint32_t *affinity_configs) {
        doca_error_t result = doca_pcc_set_thread_affinity(inner_pcc, num_threads, affinity_configs);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    void pcc::start() {
        doca_error_t result = doca_pcc_start(inner_pcc);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    void pcc::stop() {
        doca_error_t result = doca_pcc_stop(inner_pcc);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    doca_error_t pcc::wait(int wait_time) {
        return doca_pcc_wait(inner_pcc, wait_time);
    }

    doca_pcc_process_state_t pcc::get_process_state() {
        doca_pcc_process_state_t state;
        doca_error_t result = doca_pcc_get_process_state(inner_pcc, &state);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        return state;
    }

    [[nodiscard]] std::string pcc::get_type_name() const {
        return "doca_pcc";
    };

}