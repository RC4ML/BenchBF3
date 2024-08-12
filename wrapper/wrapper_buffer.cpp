#include "wrapper_buffer.h"

DOCA_LOG_REGISTER(WRAPPER_BUFFER);

namespace DOCA {

    buf_inventory::buf_inventory(uint32_t max_buf) {
        doca_error_t result = doca_buf_inventory_create(max_buf, &inner_buf_inventory);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        start();
    }

    buf_inventory::~buf_inventory() noexcept(false) {
        doca_error_t result = doca_buf_inventory_destroy(inner_buf_inventory);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("buf_inventory destroyed");
    }

    [[nodiscard]] std::string buf_inventory::get_type_name() const {
        return "doca_buf_inventory";
    }

    void buf_inventory::start() {
        doca_error_t result = doca_buf_inventory_start(inner_buf_inventory);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    [[nodiscard]] std::string buf::get_type_name() const {
        return "doca_buf";
    }
}