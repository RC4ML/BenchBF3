#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"

namespace DOCA {
    class ctx: public Base<::doca_ctx> {
    public:
        explicit ctx(engine_to_ctx *engine_to_ctx);
        // when pe call start, it will start ctx
        // and will stop after call destructor
        void start();

        inline void set_data(doca_data user_data) {
            doca_error_t result = doca_ctx_set_user_data(inner_ctx, user_data);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
        }

        inline doca_data get_data() {
            doca_data data;
            doca_error_t result = doca_ctx_get_user_data(inner_ctx, &data);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return data;
        }

        ~ctx() noexcept(false) override;

        inline ::doca_ctx *get_inner_ptr() override {
            return inner_ctx;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:

        void stop();

        ::doca_ctx *inner_ctx;
        engine_to_ctx *inner_engine_to_ctx;
    };
}