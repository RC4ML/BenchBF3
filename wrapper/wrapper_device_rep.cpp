#include "wrapper_device_rep.h"

DOCA_LOG_REGISTER(WRAPPER_DEVICE_REP);

namespace DOCA {
    devinfo_rep::devinfo_rep(::doca_devinfo_rep *devinfo_rep) {
        rt_assert(devinfo_rep != nullptr, "devinfo_rep is nullptr");
        inner_devinfo_rep = devinfo_rep;
        inner_device_rep_list = nullptr;
    }

    devinfo_rep::devinfo_rep(::doca_devinfo_rep *devinfo_rep, device_rep_list *device_list) {
        rt_assert(devinfo_rep != nullptr, "devinfo_rep is nullptr");
        inner_devinfo_rep = devinfo_rep;
        inner_device_rep_list = device_list;
    }

    devinfo_rep::~devinfo_rep() noexcept(false) {
        DOCA_LOG_DBG("devinfo_rep destroyed");
    }

    [[nodiscard]] bool devinfo_rep::is_equal(const char *pci_addr) const {
        char addr[20];
        doca_error_t result = doca_devinfo_rep_get_pci_addr_str(inner_devinfo_rep, addr);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        if (strcmp(addr, pci_addr) == 0) {
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string devinfo_rep::get_name() const {
        char pci_addr_str[20];
        doca_error_t result = doca_devinfo_rep_get_pci_addr_str(inner_devinfo_rep, pci_addr_str);
        rt_assert(result == DOCA_SUCCESS, "doca_devinfo_rep_get_pci_addr failed");

        return { pci_addr_str };
    }

    dev_rep *devinfo_rep::open() {
        return dev_rep::open(this);
    }

    ::doca_devinfo_rep *devinfo_rep::get_inner_ptr() {
        return inner_devinfo_rep;
    }

    std::string devinfo_rep::get_type_name() const {
        return "doca_devinfo_rep";
    }

    device_rep_list::device_rep_list(::doca_devinfo_rep **dev_rep_list, uint32_t num) {
        rt_assert(dev_rep_list != nullptr || (dev_rep_list == nullptr && num == 0), "dev_list_rep is nullptr");
        inner_device_rep_list_ptr = dev_rep_list;
        nb_devs = num;
        for (uint32_t i = 0; i < num; i++) {
            if (dev_rep_list[i] == nullptr) {
                break;
            }
            inner_device_rep_list.push_back(dev_rep_list[i]);
        }
    }

    device_rep_list::~device_rep_list() noexcept(false) {
        doca_devinfo_rep_destroy_list(inner_device_rep_list_ptr);
        DOCA_LOG_DBG("device_rep_list destroyed");
    }

    [[nodiscard]] size_t device_rep_list::len() const {
        return inner_device_rep_list.size();
    }

    [[nodiscard]] size_t device_rep_list::is_empty() const {
        return inner_device_rep_list.empty();
    }

    devinfo_rep device_rep_list::get(uint32_t index) {
        rt_assert(index < len(), "index out of range");

        return devinfo_rep(inner_device_rep_list[index], this);
    }

    dev_rep::dev_rep(::doca_dev_rep *dev_rep, DOCA::devinfo_rep *devinfo_rep) {
        rt_assert(dev_rep != nullptr, "dev_rep is nullptr");
        inner_dev_rep = dev_rep;
        inner_devinfo_rep = devinfo_rep;
    }

    dev_rep::~dev_rep() noexcept(false) {
        doca_dev_rep_close(inner_dev_rep);
        DOCA_LOG_DBG("dev_rep destroyed");
    }

    dev_rep *dev_rep::open(DOCA::devinfo_rep *devinfo_rep) {
        ::doca_dev_rep *dev_rep_ptr;
        doca_error_t result = doca_dev_rep_open(devinfo_rep->get_inner_ptr(), &dev_rep_ptr);
        rt_assert(result == DOCA_SUCCESS, "doca_dev_rep_open failed");
        return new dev_rep(dev_rep_ptr, devinfo_rep);
    }

    ::doca_dev_rep *dev_rep::get_inner_ptr() {
        return inner_dev_rep;
    }

    [[nodiscard]] std::string dev_rep::get_type_name() const {
        return "doca_dev_rep";
    }

    device_rep_list *get_device_rep_list(dev *local, ::doca_devinfo_rep_filter filter) {
        ::doca_devinfo_rep **dev_rep_list;
        uint32_t num;
        doca_devinfo_rep_create_list(local->get_inner_ptr(), filter, &dev_rep_list, &num);
        return new device_rep_list(dev_rep_list, num);
    }

    dev_rep *open_doca_device_rep_with_pci(dev *local, ::doca_devinfo_rep_filter filter, const char *value) {
        device_rep_list *device_rep_list = get_device_rep_list(local, filter);

        for (size_t i = 0; i < device_rep_list->len(); i++) {
            devinfo_rep devinfo_rep = device_rep_list->get(i);
            std::cout << devinfo_rep.get_name() << std::endl;
            if (devinfo_rep.is_equal(value)) {
                dev_rep *res = devinfo_rep.open();
                delete device_rep_list;
                return res;
            }
        }
        delete device_rep_list;
        throw DOCAException(DOCA_ERROR_NOT_FOUND);
    }
}