#include "wrapper_pe.h"

DOCA_LOG_REGISTER(WRAPPER_WORKQ);

namespace DOCA {

    pe::pe() {
        doca_error_t result = doca_pe_create(&inner_pe);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    pe::~pe() {
        doca_error_t result = doca_pe_destroy(inner_pe);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    void pe::connect_ctx(ctx *ctx) {
        doca_error_t result = doca_pe_connect_ctx(inner_pe, ctx->get_inner_ptr());
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        inner_ctxs.push_back(ctx);
    }

    void pe::start_ctx() {
        for (auto &ctx : inner_ctxs) {
            ctx->start();
        }
    }

    std::string pe::get_type_name() const {
        return "doca_pe";
    }

}