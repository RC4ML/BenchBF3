#pragma once

#include <bits/stdc++.h>
#include "common.hpp"

namespace DOCA {
    // all wrapper type must implement this interface
    template <typename A>
    class Base {
    public:
        virtual A *get_inner_ptr() = 0;

        virtual std::string get_type_name() const = 0;

        virtual ~Base() noexcept(false) = default;
    };

    /// Each DOCA Engine should implement their trait to
    /// transfer the engine instance into a DOCA CTX instance
    class engine_to_ctx {
    public:
        virtual ::doca_ctx *to_ctx() = 0;
    };

    class to_base_job {
    public:
        virtual ::doca_task *to_base() = 0;
    };

    //  DOCA use param to pass result,
    //  but now is 2023 and we are using c++ 20, so we just use try/catch/throw to control it!
    class DOCAException: public std::exception {
    public:
        explicit DOCAException(doca_error_t err): err_t(err) {
            ss << "DOCAException: " << doca_error_get_name(err_t) << std::endl
                << doca_error_get_descr(err_t) << std::endl;
            s_copy = ss.str();
        }

        explicit DOCAException(doca_error_t err, const char *file, int line): err_t(err) {
            ss << "DOCAException: " << doca_error_get_name(err_t) << std::endl
                << doca_error_get_descr(err_t) << "  file name:" << file << ", line:" << line << std::endl;
            s_copy = ss.str();
        }

        const char *what() const noexcept override {
            return s_copy.c_str();
        }

        doca_error_t get_error() const {
            return err_t;
        }

    private:
        doca_error_t err_t;
        std::ostringstream ss;
        std::string s_copy;
    };
}