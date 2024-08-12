#include "gflags_common.h"
#include "common.hpp"
#include <asio/ts/internet.hpp>

DOCA_LOG_REGISTER(GFLAGS_COMMON);

DEFINE_string(g_pci_address, "", "DOCA DMA device PCI address");

DEFINE_string(g_rep_pci_address, "", "DOCA DMA device representer PCI address");

DEFINE_string(g_ibdev_name, "", "DOCA device IB device name");

DEFINE_string(g_sample_text, "Hello World!", "Sample text to use");

DEFINE_string(g_network_address, "", "listen address, like ip:host");

DEFINE_uint32(g_thread_num, 1, "thread count for test");

DEFINE_bool(g_is_exported, false, "whether export or import");

DEFINE_bool(g_is_server, false, "whether server or client");

DEFINE_bool(g_is_read, false, "whether read or write");

DEFINE_uint32(g_payload, 64, "");

DEFINE_uint32(g_batch_size, 32, "");

DEFINE_uint32(g_mmap_length, 10485760, "");

DEFINE_uint32(g_life_time, 15, "");


bool IsValidPciAddress(const std::string &address) {
    std::regex pattern("^([0-9A-Fa-f]{4}:)?[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}\\.[0-9A-Fa-f]$");
    return std::regex_match(address, pattern);
}

static bool ValidatePCIAddress(const char *flag_name, const std::string &value) {
    if (value.empty()) {
        return true;
    }
    if (!IsValidPciAddress(value)) {
        DOCA_LOG_ERR("Invalid value for --%s: %s\n", flag_name, value.c_str());
        return false;
    }
    return true;
}

static bool ValidateSampleText(const char *flag_name, const std::string &value) {
    _unused(flag_name);
    _unused(value);
    return true;
}

static bool ValidateIBdevName(const char *flag_name, const std::string &value) {
    _unused(flag_name);
    if (value.empty()) {
        return true;
    }
    return value.find("mlx5_") == 0;
}

static bool ValidateNetworkAddress(const char *flag_name, const std::string &value) {
    if (value.empty()) {
        return true;
    }
    const std::regex re(R"(^(\d{1,3}\.){3}\d{1,3}:\d+$)");

    if (!std::regex_match(value, re)) {
        return false;
    }
    const auto colon_pos = value.rfind(':');
    const auto ip_str = value.substr(0, colon_pos);
    const auto port_str = value.substr(colon_pos + 1);

    asio::error_code ec;
    asio::ip::address::from_string(ip_str, ec);
    if (ec) {
        DOCA_LOG_ERR("Invalid value for --%s: %s\n", flag_name, value.c_str());
        return false;
    }

    try {
        const auto port = std::stoi(port_str);
        if (port <= 0 || port > 65535) {
            return false;
        }
    }
    catch (...) {
        return false;
    }
    return true;
}

static bool ValidateThreadNum(const char *flag_name, uint32_t value) {
    // if (value > 100) {
    //     DOCA_LOG_ERR("Invalid value for --%s: %d\n", flag_name, value);
    //     return false;
    // }
    _unused(flag_name);
    _unused(value);
    return true;
}

static bool ValidateIsExported(const char *flag_name, bool value) {
    _unused(flag_name);
    if (value) {
        DOCA_LOG_INFO("flags use [exporter]!");
    } else {
        DOCA_LOG_INFO("flags use [importer]!");
    }

    return true;
}

static bool ValidateIsServer(const char *flag_name, bool value) {
    _unused(flag_name);
    if (value) {
        DOCA_LOG_INFO("flags use [server]!");
    } else {
        DOCA_LOG_INFO("flags use [client]!");
    }
    return true;
}

static bool ValidateIsRead(const char *flag_name, bool value) {
    _unused(flag_name);
    if (value) {
        DOCA_LOG_INFO("flags use [read]!");
    } else {
        DOCA_LOG_INFO("flags use [write]!");
    }
    return true;
}

static bool ValidatePayload(const char *flag_name, uint32_t value) {
    _unused(flag_name);
    _unused(value);
    return true;
}

static bool ValidateBatchSize(const char *flag_name, uint32_t value) {
    _unused(flag_name);
    _unused(value);
    return true;
}

static bool ValidateMmapLength(const char *flag_name, uint32_t value) {
    _unused(flag_name);
    _unused(value);
    return true;
}

static bool ValidateLifeTime(const char *flag_name, uint32_t value) {
    _unused(flag_name);
    _unused(value);
    return true;
}

DEFINE_validator(g_pci_address, &ValidatePCIAddress);
DEFINE_validator(g_rep_pci_address, &ValidatePCIAddress);
DEFINE_validator(g_ibdev_name, &ValidateIBdevName);
DEFINE_validator(g_sample_text, &ValidateSampleText);
DEFINE_validator(g_network_address, &ValidateNetworkAddress);
DEFINE_validator(g_thread_num, &ValidateThreadNum);
DEFINE_validator(g_is_exported, &ValidateIsExported);
DEFINE_validator(g_is_server, &ValidateIsServer);
DEFINE_validator(g_is_read, &ValidateIsRead);
DEFINE_validator(g_payload, &ValidatePayload);
DEFINE_validator(g_batch_size, &ValidateBatchSize);
DEFINE_validator(g_mmap_length, &ValidateMmapLength);
DEFINE_validator(g_life_time, &ValidateLifeTime);
