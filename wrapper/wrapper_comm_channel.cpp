#include "wrapper_comm_channel.h"

DOCA_LOG_REGISTER(WRAPPER_COMM_CHANNEL);

namespace DOCA {
    comm_channel_addr::comm_channel_addr(DOCA::comm_channel_ep *ep, ::doca_comm_channel_addr_t *addr) {
        rt_assert(ep != nullptr, "ep is nullptr");
        rt_assert(addr != nullptr, "addr is nullptr");

        inner_addr = addr;
        inner_ep = ep;
    }

    comm_channel_addr::~comm_channel_addr() noexcept(false) {
        doca_error_t result = doca_comm_channel_ep_disconnect(inner_ep->get_inner_ptr(), inner_addr);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("comm_channel_addr destroyed");
    }

    [[nodiscard]] std::string comm_channel_addr::get_type_name() const {
        return "doca_comm_channel_addr_t";
    }

    comm_channel_ep::comm_channel_ep(dev *dev, dev_rep *dev_rep, const std::string &name) {
        rt_assert(dev != nullptr, "dev is nullptr");
//        rt_assert(dev_rep != nullptr, "dev_rep is nullptr");

        inner_dev = dev;
        inner_dev_rep = dev_rep;
        server_name = name;
        stop_flag = false;
        doca_error_t result = doca_comm_channel_ep_create(&inner_ep);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }

        result = doca_comm_channel_ep_set_device(inner_ep, dev->get_inner_ptr());
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }

        if (dev_rep != nullptr) {
            result = doca_comm_channel_ep_set_device_rep(inner_ep, dev_rep->get_inner_ptr());
            if (result != DOCA_SUCCESS) {
                throw DOCAException(result, __FILE__, __LINE__);
            }
        }
    }

    comm_channel_ep::~comm_channel_ep() noexcept(false) {
        doca_error_t result = doca_comm_channel_ep_destroy(inner_ep);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("comm_channel_ep destroyed");
    }

    void comm_channel_ep::set_property(std::vector<uint16_t> property) {
        rt_assert(property.size() == 3, "property size must be 3, please see [set_property] comments");
        doca_error_t result;
        if (property[0] > 0) {
            result = doca_comm_channel_ep_set_max_msg_size(inner_ep, property[0]);
            if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Failed to set max_msg_size property, continue...");
            }
        }

        if (property[1] > 0) {
            result = doca_comm_channel_ep_set_send_queue_size(inner_ep, property[1]);
            if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Failed to set send_queue_size property, continue...");
            }
        }

        if (property[2] > 0) {
            result = doca_comm_channel_ep_set_recv_queue_size(inner_ep, property[2]);
            if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Failed to set recv_queue_size property, continue...");
            }
        }
    }

    std::vector<uint16_t> comm_channel_ep::get_property() {
        uint16_t size = 0;
        doca_error_t result;
        std::vector<uint16_t> res;
        result = doca_comm_channel_ep_get_max_msg_size(inner_ep, &size);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to get max_msg_size property, continue...");
            res.push_back(0);
        } else {
            res.push_back(size);
        }

        result = doca_comm_channel_ep_get_send_queue_size(inner_ep, &size);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to get send_queue_size property, continue...");
            res.push_back(0);
        } else {
            res.push_back(size);
        }

        result = doca_comm_channel_ep_get_recv_queue_size(inner_ep, &size);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to get recv_queue_size property, continue...");
            res.push_back(0);
        } else {
            res.push_back(size);
        }
        return res;
    }

    void comm_channel_ep::start_listen() {
        doca_error_t result = doca_comm_channel_ep_listen(inner_ep, server_name.c_str());
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        DOCA_LOG_DBG("comm_channel_ep started listening");
    }

    void comm_channel_ep::stop_recv_and_send() {
        stop_flag = true;
        DOCA_LOG_DBG("stop recv and send");
    }

    comm_channel_addr *comm_channel_ep::ep_connect() {
        doca_error_t result;
        ::doca_comm_channel_addr_t *peer_addr = nullptr;
        result = doca_comm_channel_ep_connect(inner_ep, server_name.c_str(), &peer_addr);
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        while ((result = doca_comm_channel_peer_addr_update_info(peer_addr)) == DOCA_ERROR_CONNECTION_INPROGRESS) {
            if (stop_flag) {
                result = DOCA_ERROR_UNEXPECTED;
                break;
            }
            usleep(1);
        }
        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }
        return new comm_channel_addr(this, peer_addr);
    }

    comm_channel_addr *comm_channel_ep::ep_recvfrom(void *recv_buf, size_t *recv_len) {
        doca_error_t result;
        ::doca_comm_channel_addr_t *peer_addr = nullptr;
        while ((result = doca_comm_channel_ep_recvfrom(inner_ep, recv_buf, recv_len, DOCA_CC_MSG_FLAG_NONE,
                                                       &peer_addr)) == DOCA_ERROR_AGAIN) {
            if (stop_flag) {
                result = DOCA_ERROR_UNEXPECTED;
                break;
            }
            //usleep(1);
        }

        if (result != DOCA_SUCCESS) {
            throw DOCAException(result, __FILE__, __LINE__);
        }

        return new comm_channel_addr(this, peer_addr);
    }

    bool comm_channel_ep::ep_recvfrom(void *recv_buf, size_t *recv_len, comm_channel_addr *addr) {
        rt_assert(addr != nullptr, "addr is nullptr, please use ep_recvfrom without addr parameter version");
        doca_error_t result;
        while ((result = doca_comm_channel_ep_recvfrom(inner_ep, recv_buf, recv_len, DOCA_CC_MSG_FLAG_NONE,
                                                       &addr->inner_addr)) == DOCA_ERROR_AGAIN) {
            if (unlikely(stop_flag)) {
                result = DOCA_ERROR_UNEXPECTED;
                break;
            }
            //usleep(1);
        }
        if (likely(result == DOCA_SUCCESS)) {
            return true;
        } else if (unlikely(result == DOCA_ERROR_UNEXPECTED)) {
            DOCA_LOG_WARN("stop and exit from recv");
            return false;
        } else {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    doca_error_t comm_channel_ep::ep_recvfrom_async(void *recv_buf, size_t *recv_len, comm_channel_addr *addr) {
        return doca_comm_channel_ep_recvfrom(inner_ep, recv_buf, recv_len, DOCA_CC_MSG_FLAG_NONE,
                                             &addr->inner_addr);
    }

    bool comm_channel_ep::ep_sendto(const void *send_buf, size_t send_len, comm_channel_addr *addr) {
        doca_error_t result;
        while ((result = doca_comm_channel_ep_sendto(inner_ep, send_buf, send_len, DOCA_CC_MSG_FLAG_NONE,
                                                     addr->get_inner_ptr())) == DOCA_ERROR_AGAIN) {
            if (stop_flag) {
                result = DOCA_ERROR_UNEXPECTED;
                break;
            }
            //usleep(1);
        }

        if (likely(result == DOCA_SUCCESS)) {
            return true;
        } else if (unlikely(result == DOCA_ERROR_UNEXPECTED)) {
            DOCA_LOG_WARN("stop and exit from recv");
            return false;
        } else {
            throw DOCAException(result, __FILE__, __LINE__);
        }
    }

    ::doca_comm_channel_ep_t *comm_channel_ep::get_inner_ptr() {
        return inner_ep;
    }

    [[nodiscard]] std::string comm_channel_ep::get_type_name() const {
        return "doca_comm_channel_ep_t";
    }

}