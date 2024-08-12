#include <doca_log.h>

struct doca_log_backend *app_logger = nullptr;
struct doca_log_backend *sdk_logger = nullptr;
static int __unused = []() {
    if (app_logger != nullptr || sdk_logger != nullptr) {
        return 0;
    }
    doca_error_t result = doca_log_backend_create_with_file(stdout, &app_logger);
    if (result != DOCA_SUCCESS) {
        return -1;
    }
    result = doca_log_backend_create_with_file_sdk(stderr, &sdk_logger);
    if (result != DOCA_SUCCESS) {
        return -1;
    }
    doca_log_backend_set_level_lower_limit(app_logger, DOCA_LOG_LEVEL_INFO);
    doca_log_backend_set_sdk_level(sdk_logger, DOCA_LOG_LEVEL_WARNING);
    return 0;
    }();