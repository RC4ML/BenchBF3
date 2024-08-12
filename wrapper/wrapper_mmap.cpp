#include "wrapper_mmap.h"

DOCA_LOG_REGISTER(WRAPPER_MMAP);

namespace DOCA {


    mmap::mmap(bool rm_before_close): ok(rm_before_close) {
        doca_error_t result = doca_mmap_create(&inner_mmap);

        rt_assert(result == DOCA_SUCCESS, "doca_mmap_create failed");

        // start();
    }

    mmap::mmap(): ok(false) {

    }

    mmap::~mmap() noexcept(false) {
        doca_error_t result;
        // doca_mmap_dev_rm will get DOCA_ERROR_BAD_STATE in DOCA 2
        // if (ok) {
        //     for (auto dev: added_devs) {
        //         result = doca_mmap_dev_rm(inner_mmap, dev->get_inner_ptr());
        //         if (result != DOCA_SUCCESS) {
        //             throw DOCAException(result, __FILE__, __LINE__);
        //         }
        //     }
        // }
        result = doca_mmap_destroy(inner_mmap);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("mmap destroyed");
    }

    void mmap::set_memrange(char *memptr, size_t num) {
        doca_error_t result = doca_mmap_set_memrange(inner_mmap, memptr, num);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    void mmap::set_memrange_and_start(char *memptr, size_t num) {
        set_memrange(memptr, num);
        start();
    }

    std::string mmap::export_dpu(size_t dev_index) {
        if (dev_index >= added_devs.size()) {
            throw DOCAException(DOCA_ERROR_BAD_STATE, __FILE__, __LINE__);
        }
        const void *export_desc;
        size_t export_desc_len;
        doca_error_t error = doca_mmap_export_pci(inner_mmap, added_devs[dev_index]->get_inner_ptr(), &export_desc, &export_desc_len);
        if (error != DOCA_SUCCESS) {
            throw DOCAException(error, __FILE__, __LINE__);
        }
        return std::string(reinterpret_cast<const char *>(export_desc), export_desc_len);
    }

    std::vector<std::string> mmap::export_dpus() {
        std::vector<std::string >res;
        for (size_t i = 0; i < added_devs.size(); i++) {
            res.push_back(export_dpu(i));
        }
        return res;
    }

    void mmap::add_device(dev *dev_ptr) {
        doca_error_t result = doca_mmap_add_dev(inner_mmap, dev_ptr->get_inner_ptr());
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        added_devs.push_back(dev_ptr);
    }

    void mmap::start() {
        doca_error_t result = doca_mmap_start(inner_mmap);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    void mmap::set_permissions(uint32_t permission) {
        doca_error_t result = doca_mmap_set_permissions(inner_mmap, permission);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    mmap *mmap::new_from_exported_dpu(std::string &exporter, dev *dev) {
        mmap *res = new mmap();
        doca_error_t result = doca_mmap_create_from_export(nullptr, exporter.c_str(), exporter.size(), dev->get_inner_ptr(), &res->inner_mmap);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        res->added_devs.push_back(dev);
        return res;
    }

    std::string mmap::get_type_name() const {
        return "doca_mmap";
    }
}