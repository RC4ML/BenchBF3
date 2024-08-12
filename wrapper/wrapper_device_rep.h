#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"

namespace DOCA {
    class device_rep_list;

    class dev_rep;

    class devinfo_rep: public Base<::doca_devinfo_rep> {
    public:
        explicit devinfo_rep(::doca_devinfo_rep *devinfo_rep);

        explicit devinfo_rep(::doca_devinfo_rep *devinfo_rep, device_rep_list *device_list);

        ~devinfo_rep() noexcept(false) override;

        [[nodiscard]] bool is_equal(const char *pci_addr) const;

        [[nodiscard]] std::string get_name() const;

        dev_rep *open();

        ::doca_devinfo_rep *get_inner_ptr() override;

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_devinfo_rep *inner_devinfo_rep;
        device_rep_list *inner_device_rep_list;
    };

    class device_rep_list {
    public:
        explicit device_rep_list(::doca_devinfo_rep **dev_rep_list, uint32_t num);

        ~device_rep_list() noexcept(false);

        [[nodiscard]] size_t len() const;

        [[nodiscard]] size_t is_empty() const;

        devinfo_rep get(uint32_t index);

    private:
        std::vector<::doca_devinfo_rep *> inner_device_rep_list;
        ::doca_devinfo_rep **inner_device_rep_list_ptr;
        uint32_t nb_devs;
    };

    class dev_rep: public Base<::doca_dev_rep> {
    public:
        explicit dev_rep(::doca_dev_rep *dev_rep, devinfo_rep *devinfo_rep);

        ~dev_rep() noexcept(false) override;

        static dev_rep *open(devinfo_rep *devinfo_rep);

        ::doca_dev_rep *get_inner_ptr() override;

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_dev_rep *inner_dev_rep;
        devinfo_rep *inner_devinfo_rep;
    };

    device_rep_list *get_device_rep_list(dev *local, ::doca_devinfo_rep_filter filter);

    dev_rep *open_doca_device_rep_with_pci(dev *local, ::doca_devinfo_rep_filter filter, const char *value);
}