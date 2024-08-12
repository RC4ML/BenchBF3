#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"

namespace DOCA {

    class mmap: public Base<::doca_mmap> {
    public:
        explicit mmap(bool rm_before_close);

        explicit mmap();

        ~mmap() noexcept(false) override;

        void set_memrange(char *memptr, size_t num);

        void set_memrange_and_start(char *memptr, size_t num);

        std::string export_dpu(size_t dev_index);

        std::vector<std::string> export_dpus();

        void add_device(dev *dev_ptr);

        void start();

        void set_permissions(uint32_t permission);

        static mmap *new_from_exported_dpu(std::string &exporter, dev *dev);

        inline ::doca_mmap *get_inner_ptr() override {
            return inner_mmap;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_mmap *inner_mmap{};
        std::vector<dev *> added_devs;
        bool ok{};
    };
}