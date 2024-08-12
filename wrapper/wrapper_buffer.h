#pragma once

#include "wrapper_interface.h"
#include "wrapper_mmap.h"

namespace DOCA {
    class buf_inventory: public Base<::doca_buf_inventory> {

    public:
        explicit buf_inventory(uint32_t max_buf);

        ~buf_inventory() noexcept(false) override;

        inline ::doca_buf_inventory *get_inner_ptr() override {
            return inner_buf_inventory;
        }

        [[nodiscard]] std::string get_type_name() const override;

    private:
        void start();

        ::doca_buf_inventory *inner_buf_inventory{};
    };

    class buf: public Base<::doca_buf> {
    public:
        inline explicit buf(::doca_buf *buf, void *rp, size_t rs, buf_inventory *bip, mmap *mp) {
            inner_buf = buf;
            raw_pointer = rp;
            raw_size = rs;
            buf_inventory_ptr = bip;
            mmap_ptr = mp;
            // TODO: where to call doca_buf_create?
            // FIXME!
        }

        inline ~buf() noexcept(false) override {
            doca_error_t result = doca_buf_dec_refcount(inner_buf, nullptr);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            // DOCA_LOG_DBG("buf destroyed");
        }

        inline void *get_data() {
            void *res;
            doca_error_t result = doca_buf_get_data(inner_buf, &res);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return res;
        }

        inline size_t get_length() {
            size_t res;
            doca_error_t result = doca_buf_get_data_len(inner_buf, &res);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return res;
        }

        inline void set_data(size_t offset, size_t data_size) {
            doca_error_t result = doca_buf_set_data(inner_buf, static_cast<char *>(raw_pointer) + offset, data_size);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
        }

        inline void set_data(void *data_ptr, size_t data_size) {
            doca_error_t result = doca_buf_set_data(inner_buf, data_ptr, data_size);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
        }

        inline void buf_append(buf *list_append) {
            doca_error_t result = doca_buf_chain_list(inner_buf, list_append->get_inner_ptr());
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
        }

        inline static buf *
            buf_inventory_buf_by_addr(buf_inventory *buf_inventory_ptr, mmap *mmap_ptr, void *addr, size_t len) {
            ::doca_buf *i_buf;
            doca_error_t result = doca_buf_inventory_buf_get_by_addr(buf_inventory_ptr->get_inner_ptr(),
                mmap_ptr->get_inner_ptr(), addr, len, &i_buf);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return new buf(i_buf, addr, len, buf_inventory_ptr, mmap_ptr);
        }

        inline static buf *
            buf_inventory_buf_by_data(buf_inventory *buf_inventory_ptr, mmap *mmap_ptr, void *addr, size_t len) {
            ::doca_buf *i_buf;
            doca_error_t result = doca_buf_inventory_buf_get_by_data(buf_inventory_ptr->get_inner_ptr(),
                mmap_ptr->get_inner_ptr(), addr, len, &i_buf);
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
            return new buf(i_buf, addr, len, buf_inventory_ptr, mmap_ptr);
        }

        inline ::doca_buf *get_inner_ptr() override {
            return inner_buf;
        }

        [[nodiscard]] std::string get_type_name() const override;

        void *raw_pointer;
        size_t raw_size;

        // following two fields not used for now, but it's here for future use
        buf_inventory *buf_inventory_ptr;
        mmap *mmap_ptr;

    private:
        ::doca_buf *inner_buf{};
    };

}