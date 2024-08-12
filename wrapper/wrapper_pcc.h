#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"

namespace DOCA {

    class pcc : public Base<::doca_pcc> {
    public:
        explicit pcc(dev *dev);

        ~pcc() noexcept(false) override;

        // return min_num_threads and max_num_threads
        std::pair<uint32_t, uint32_t> get_num_threads();

        void set_app(doca_pcc_app *app);

        void set_thread_affinity(uint32_t num_threads, uint32_t *affinity_configs);

        void start();

        void stop();

        doca_error_t wait(int wait_time);

        doca_pcc_process_state_t get_process_state();

        inline ::doca_pcc *get_inner_ptr() override {
            return inner_pcc;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_pcc *inner_pcc{};
    };
}