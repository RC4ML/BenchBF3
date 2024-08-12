#pragma once

#include "wrapper_interface.h"

namespace DOCA {
    using jobs_check = std::function<doca_error_t(struct doca_devinfo *)>;

    class device_list;

    class dev;

    class devinfo: public Base<::doca_devinfo> {
    public:
        explicit devinfo(::doca_devinfo *devinfo);

        explicit devinfo(::doca_devinfo *devinfo, device_list *device_list);

        ~devinfo() noexcept(false) override;

        [[nodiscard]] bool is_equal(const char *pci_addr) const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] std::string get_ibv_name() const;

        [[nodiscard]] size_t get_max_buf_size() const;

        dev *open();

        inline ::doca_devinfo *get_inner_ptr() override {
            return inner_devinfo;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_devinfo *inner_devinfo;
        device_list *inner_device_list;
    };

    class device_list {
    public:
        explicit device_list(::doca_devinfo **dev_list, uint32_t num);

        ~device_list() noexcept(false);

        [[nodiscard]] size_t len() const;

        [[nodiscard]] size_t is_empty() const;

        devinfo get(uint32_t index);

    private:
        std::vector<::doca_devinfo *> inner_device_list;
        ::doca_devinfo **inner_device_list_ptr;
        uint32_t nb_devs;
    };

    class dev: public Base<::doca_dev> {

    public:
        explicit dev(::doca_dev *dev, devinfo *devinfo);

        ~dev() noexcept(false) override;

        static dev *open(devinfo *devinfo);

        inline ::doca_dev *get_inner_ptr() override {
            return inner_dev;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_dev *inner_dev;
        devinfo *inner_devinfo;
    };

    device_list *get_device_list();

    dev *open_doca_device_with_pci(const char *value, const jobs_check &func);

    dev *open_doca_device_with_ibdev_name(const char *value, const jobs_check &func);
}