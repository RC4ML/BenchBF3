#include "wrapper_device.h"

DOCA_LOG_REGISTER(WRAPPER_DEVICE);

namespace DOCA {
    devinfo::devinfo(::doca_devinfo *devinfo) {
        rt_assert(devinfo != nullptr, "devinfo is nullptr");
        inner_devinfo = devinfo;
        inner_device_list = nullptr;
    }

    devinfo::devinfo(::doca_devinfo *devinfo, device_list *device_list) {
        rt_assert(devinfo != nullptr, "devinfo is nullptr");
        inner_devinfo = devinfo;
        inner_device_list = device_list;
    }

    devinfo::~devinfo() noexcept(false) {
        DOCA_LOG_DBG("devinfo destroyed");
    }

    [[nodiscard]] bool devinfo::is_equal(const char *pci_addr) const {
        uint8_t is_addr_equal = 0;
        doca_error_t result = doca_devinfo_is_equal_pci_addr(inner_devinfo, pci_addr, &is_addr_equal);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        return is_addr_equal;
    }

    [[nodiscard]] std::string devinfo::get_name() const {
        char pci_addr_str[20];
        doca_error_t result = doca_devinfo_get_pci_addr_str(inner_devinfo, pci_addr_str);
        rt_assert(result == DOCA_SUCCESS, "doca_devinfo_rep_get_pci_addr failed");

        return { pci_addr_str };
    }

    [[nodiscard]] std::string devinfo::get_ibv_name() const {
        char ibv_name[DOCA_DEVINFO_IBDEV_NAME_SIZE];
        doca_error_t result = doca_devinfo_get_ibdev_name(inner_devinfo, ibv_name, DOCA_DEVINFO_IBDEV_NAME_SIZE);
        rt_assert(result == DOCA_SUCCESS, "doca_devinfo_get_ibv_name failed");
        return { ibv_name };
    }

    [[nodiscard]] size_t devinfo::get_max_buf_size() const {
        size_t max_buf_size;
        doca_error_t result = doca_dma_cap_task_memcpy_get_max_buf_size(inner_devinfo, &max_buf_size);
        rt_assert(result == DOCA_SUCCESS, "doca_devinfo_get_max_buf_size failed");
        return max_buf_size;
    }

    dev *devinfo::open() {
        return dev::open(this);
    }

    std::string devinfo::get_type_name() const {
        return "doca_devinfo";
    }

    device_list::device_list(::doca_devinfo **dev_list, uint32_t num) {
        rt_assert(dev_list != nullptr || (dev_list == nullptr && num == 0), "dev_list is nullptr");
        inner_device_list_ptr = dev_list;
        nb_devs = num;
        for (uint32_t i = 0; i < num; i++) {
            inner_device_list.push_back(dev_list[i]);
        }
    }

    device_list::~device_list() noexcept(false) {
        doca_devinfo_destroy_list(inner_device_list_ptr);
        DOCA_LOG_DBG("device_list destroyed");
    }

    [[nodiscard]] size_t device_list::len() const {
        return inner_device_list.size();
    }

    [[nodiscard]] size_t device_list::is_empty() const {
        return inner_device_list.empty();
    }

    devinfo device_list::get(uint32_t index) {
        rt_assert(index < len(), "index out of range");

        return devinfo(inner_device_list[index], this);
    }

    dev::dev(::doca_dev *dev, devinfo *devinfo) {
        rt_assert(dev != nullptr, "dev is nullptr");
        inner_dev = dev;
        inner_devinfo = devinfo;
    }

    dev::~dev() noexcept(false) {
        doca_dev_close(inner_dev);
        DOCA_LOG_DBG("dev destroyed");
    }

    dev *dev::open(devinfo *devinfo) {
        ::doca_dev *dev_ptr;
        doca_error_t result = doca_dev_open(devinfo->get_inner_ptr(), &dev_ptr);
        rt_assert(result == DOCA_SUCCESS, "doca_dev_open failed");
        return new dev(dev_ptr, devinfo);
    }

    [[nodiscard]] std::string dev::get_type_name() const {
        return "doca_dev";
    }

    device_list *get_device_list() {
        ::doca_devinfo **dev_list;
        uint32_t num;
        doca_devinfo_create_list(&dev_list, &num);
        return new device_list(dev_list, num);
    }

    dev *open_doca_device_with_pci(const char *value, const jobs_check &func) {
        device_list *device_list = get_device_list();

        for (size_t i = 0; i < device_list->len(); i++) {
            devinfo devinfo = device_list->get(i);
            if (devinfo.is_equal(value)) {
                if (func == nullptr || func(devinfo.get_inner_ptr()) == DOCA_SUCCESS) {
                    // warning::
                    //  must open before delete device_list
                    //  because devinfo.open need use devinfo ptr, and delte device_list will free devinfo ptr
                    dev *res = devinfo.open();
                    delete device_list;
                    return res;
                }
            }
        }
        delete device_list;
        throw DOCAException(DOCA_ERROR_NOT_FOUND, __FILE__, __LINE__);
    }

    dev *open_doca_device_with_ibdev_name(const char *value, const jobs_check &func) {
        device_list *device_list = get_device_list();

        for (size_t i = 0; i < device_list->len(); i++) {
            devinfo devinfo = device_list->get(i);
            if (devinfo.get_ibv_name() == value) {
                if (func == nullptr || func(devinfo.get_inner_ptr()) == DOCA_SUCCESS) {
                    // warning::
                    //  must open before delete device_list
                    //  because devinfo.open need use devinfo ptr, and delte device_list will free devinfo ptr
                    dev *res = devinfo.open();
                    delete device_list;
                    return res;
                }
            }
        }
        delete device_list;
        throw DOCAException(DOCA_ERROR_NOT_FOUND, __FILE__, __LINE__);
    }
}