#pragma once

#include "wrapper_interface.h"
#include "wrapper_device.h"
#include "wrapper_device_rep.h"

namespace DOCA {
    class comm_channel_ep;

    class comm_channel_addr : public Base<::doca_comm_channel_addr_t> {
    public:
        explicit comm_channel_addr(comm_channel_ep *ep, ::doca_comm_channel_addr_t *addr);

        ~comm_channel_addr() noexcept(false) override;

        inline ::doca_comm_channel_addr_t *get_inner_ptr() override {
            return inner_addr;
        }

        [[nodiscard]] std::string get_type_name() const override;

        ::doca_comm_channel_addr_t *inner_addr;
        comm_channel_ep *inner_ep;
    };

    class comm_channel_ep : public Base<::doca_comm_channel_ep_t> {
    public:
        explicit comm_channel_ep(dev *dev, dev_rep *dev_rep, const std::string &name);

        ~comm_channel_ep() noexcept(false) override;

        // get max_msg_size, send_queue_size, recv_queue_size, at 0,1,2 entry
        std::vector<uint16_t> get_property();

        // set max_msg_size, send_queue_size, recv_queue_size, at 0,1,2 entry, 0 means not change
        void set_property(std::vector<uint16_t> property);

        // start listern (only use for server)
        void start_listen();

        // both use for server and client
        void stop_recv_and_send();

        comm_channel_addr *ep_connect();

        // used for first time communication
        comm_channel_addr *ep_recvfrom(void *recv_buf, size_t *recv_len);

        //used for later communication
        bool ep_recvfrom(void *recv_buf, size_t *recv_len, comm_channel_addr *addr);

        doca_error_t ep_recvfrom_async(void *recv_buf, size_t *recv_len, comm_channel_addr *addr);

        bool ep_sendto(const void *send_buf, size_t send_len, comm_channel_addr *addr);


        ::doca_comm_channel_ep_t *get_inner_ptr() override;

        [[nodiscard]] std::string get_type_name() const override;

    private:
        ::doca_comm_channel_ep_t *inner_ep;
        dev *inner_dev;
        dev_rep *inner_dev_rep;
        std::string server_name;
        std::atomic<bool> stop_flag;
    };
}