#include "wrapper_ctx.h"

DOCA_LOG_REGISTER(WRAPPER_CTX);

namespace DOCA {
    ctx::ctx(engine_to_ctx *engine_to_ctx) {
        try {
            rt_assert(engine_to_ctx != nullptr, "engine_to_ctx is nullptr");
            inner_engine_to_ctx = engine_to_ctx;
            inner_ctx = engine_to_ctx->to_ctx();
        }
        catch (DOCAException &e) {
            DOCA_LOG_ERR("%s", e.what());
            exit(1);
        }
    }

    ctx::~ctx() noexcept(false) {
        try {
            stop();
        }
        catch (DOCAException &e) {
            DOCA_LOG_ERR("%s", e.what());
            exit(1);
        }
        DOCA_LOG_DBG("ctx destroyed");
    }

    [[nodiscard]] std::string ctx::get_type_name() const {
        return "doca_ctx";
    }

    void ctx::start() {
        doca_error_t result = doca_ctx_start(inner_ctx);
        // DOCA_ERROR_IN_PROGRESS is OK in RDMA
        if (result != DOCA_SUCCESS && result != DOCA_ERROR_IN_PROGRESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    void ctx::stop() {
        doca_error_t result = doca_ctx_stop(inner_ctx);
        // DOCA_ERROR_IN_PROGRESS is OK in RDMA
        if (result != DOCA_SUCCESS && result != DOCA_ERROR_IN_PROGRESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }
}