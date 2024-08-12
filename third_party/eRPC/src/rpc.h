#pragma once

#include <map>
#include <set>
#include "cc/timing_wheel.h"
#include "common.h"
#include "msg_buffer.h"
#include "nexus.h"
#include "pkthdr.h"
#include "req_handle.h"
#include "rpc_types.h"
#include "session.h"
#include "transport.h"
#include "util/buffer.h"
#include "util/fixed_queue.h"
#include "util/huge_alloc.h"
#include "util/logger.h"
#include "util/mt_queue.h"
#include "util/rand.h"
#include "util/timer.h"
#include "util/udp_client.h"
#include "util/spin_lock_mutex.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

namespace erpc
{

  /**
   * \mainpage
   *
   * eRPC is a fast remote procedure call library designed for datacenter
   * networks. The <a href="https://github.com/efficient/eRPC">source code</a> is
   * maintained on GitHub. Our USENIX NSDI
   * <a href="http://www.cs.cmu.edu/~akalia/doc/nsdi19/erpc_nsdi19.pdf">paper</a>
   * describes eRPC's design in detail.
   *
   * See the API documentation tab for details on how to use eRPC. Using eRPC
   * requires the following steps:
   *
   * -# Create a Nexus to initialize the eRPC library
   * -# Register request handlers using Nexus::register_req_func
   * -# For an RPC server thread:
   *    - Create an Rpc end point using the Nexus
   *    - Run the event loop with Rpc::run_event_loop. The request handler will
   *      be invoked when requests are received from clients.
   *    - Create the response in the request handle supplied to the handler.
   *      Use Rpc::enqueue_response with either of the two response handle msgbufs
   *      (pre_resp_msgbuf_ or dyn_resp_msgbuf_) to send the reply.
   * -# For an RPC client thread:
   *    - Create an Rpc endpoint using the Nexus
   *    - Connect to remote endpoints with Rpc::create_session, specifying the
   *      server's "control network" hostname. The connection handshake is done
   *      over the kernel's network stack.
   *    - Create buffers to store request and response messages with
   *      Rpc::alloc_msg_buffer
   *    - Send a request with Rpc::enqueue_request, specifying a continuation
   *      callback
   *    - Run eRPC's event loop with Rpc::run_event_loop
   *    - The continuation will run when the response is received, or when the
   *      request times out
   */

  /**
   * @brief An Rpc object is the main communication end point in eRPC.
   * Applications use it to create sessions with remote Rpc objects, send and
   * receive requests and responses, and run the event loop.
   *
   * None of the functions are thread safe for user threads.
   *
   * eRPC's worker (background) threads have limited; concurrent access to Rpc
   * objects. The functions with the _st can be called from only the foreground
   * thread that owns the Rpc object.
   *
   * @tparam TTr The unreliable transport
   */
  template <class TTr>
  class Rpc
  {
    friend class RpcTest;

  private:
    /// Initial capacity of the hugepage allocator
    static constexpr size_t kInitialHugeAllocSize = MB(8);

    /// Timeout for a session management request in milliseconds
    static constexpr size_t kSMTimeoutMs = kTesting ? 10 : 100;

  public:
    /// Max request or response *data* size, i.e., excluding packet headers
    static constexpr size_t kMaxMsgSize =
        HugeAlloc::k_max_class_size -
        ((HugeAlloc::k_max_class_size / TTr::kMaxDataPerPkt) * sizeof(pkthdr_t));
    static_assert((1 << kMsgSizeBits) >= kMaxMsgSize, "");
    static_assert((1 << kPktNumBits) * TTr::kMaxDataPerPkt > 2 * kMaxMsgSize, "");

    /**
     * @brief Construct the Rpc object
     * @param nexus The Nexus object created by this process
     * @param context The context passed by the event loop to user callbacks
     *
     * @param rpc_id Each Rpc object created by threads of one process must
     * have a unique ID. Users create connections to remote Rpc objects by
     * specifying the URI of the remote process, and the remote Rpc's ID.
     *
     * \param sm_handler The session management callback that is invoked when
     * sessions are successfully created or destroyed.
     *
     * @param phy_port An Rpc object uses one port on a "datapath" NIC, which
     * refers to a NIC that supports DPDK or ibverbs. phy_port is the zero-based
     * index of that port among active ports, same as the one passed to
     * `rte_eth_dev_info_get` for the DPDK transport; or as listed by
     * `ibv_devinfo` for Raw, InfiniBand, and RoCE transports. Multiple Rpc
     * objects may use the same phy_port.
     *
     * @throw runtime_error if construction fails
     */
    Rpc(Nexus *nexus, void *context, uint8_t rpc_id, sm_handler_t sm_handler,
        uint8_t phy_port = 0);

    /// Destroy the Rpc from a foreground thread
    ~Rpc();

    /**
     * @brief Create a hugepage-backed buffer for storing request or response
     * messages. Safe to call from background threads (TS).
     *
     * @param max_data_size If this call is successful, the returned MsgBuffer
     * contains space for this many application data bytes. The MsgBuffer should
     * be resized with resize_msg_buffer() when used for smaller requests or
     * responses.
     *
     * @return The allocated message buffer. The returned message buffer is
     * invalid (i.e., its MsgBuffer.buf is null) if we ran out of hugepage memory.
     *
     * @throw runtime_error if \p size is too large for the allocator, or if
     * hugepage reservation failure is catastrophic. An exception is *not* thrown
     * if allocation fails simply because we ran out of memory.
     *
     * \note The returned MsgBuffer's \p buf is surrounded by packet headers for
     * internal use by eRPC. This function does not fill in packet headers,
     * although it sets the magic field in the zeroth header.
     */
    inline MsgBuffer alloc_msg_buffer(size_t max_data_size)
    {
      assert(max_data_size > 0); // Doesn't work for max_data_size = 0

      // This function avoids division for small data sizes
      size_t max_num_pkts = data_size_to_num_pkts(max_data_size);

      huge_alloc_lock_.lock();
      Buffer buffer =
          huge_alloc_->alloc(max_data_size + (max_num_pkts * sizeof(pkthdr_t)));
      huge_alloc_lock_.unlock();

      if (unlikely(buffer.buf_ == nullptr))
      {
        MsgBuffer msg_buffer;
        msg_buffer.buf_ = nullptr;
        return msg_buffer;
      }

      MsgBuffer msg_buffer(buffer, max_data_size, max_num_pkts);
      return msg_buffer;
    }

    /**
     * @brief Resize a MsgBuffer to fit a request or response. Safe to call from
     * background threads (TS).
     *
     * @param msg_buffer The MsgBuffer to resize
     *
     * @param new_data_size The new size in bytes of the application data that
     * this MsgBuffer should contain. This must be smaller than the size used
     * to create the MsgBuffer in alloc_msg_buffer().
     *
     * \p note This does not modify the MsgBuffer's packet headers
     */
    static inline void resize_msg_buffer(MsgBuffer *msg_buffer,
                                         size_t new_data_size)
    {
      assert(new_data_size <= msg_buffer->max_data_size_);

      // Avoid division for single-packet data sizes
      size_t new_num_pkts = data_size_to_num_pkts(new_data_size);
      msg_buffer->resize(new_data_size, new_num_pkts);
    }

    /// Free a MsgBuffer created by alloc_msg_buffer(). Safe to call from
    /// background threads (TS).
    inline void free_msg_buffer(MsgBuffer msg_buffer)
    {
      huge_alloc_lock_.lock();
      huge_alloc_->free_buf(msg_buffer.buffer_);
      huge_alloc_lock_.unlock();
    }

    /**
     * @brief A session is a connection between two eRPC endpoints (similar to a
     * TCP connection). This function creates a session to a remote Rpc object and
     * initiates session connection. A session management callback of type \p
     * kConnected or \p kConnectFailed will be invoked after session creation
     * completes or fails.
     *
     * Session establisment in eRPC is not optimized, and should be avoided
     * on the datapath.
     *
     * @return The local session number (>= 0) of the session if the session
     * handshake is successfully initiated, negative errno otherwise.
     *
     * @param remote_uri The remote Nexus's URI, formatted as hostname:udp_port.
     * The hostname (or IP address) should be under the remote kernel's control.
     * For example, with DPDK, the userspace driver takes full control of a port.
     * That port's IP address is meaningful only prior to its binding with DPDK,
     * so it won't work in create_session.
     *
     * @param rem_rpc_id The ID of the remote Rpc object
     */
    int create_session(std::string remote_uri, uint8_t rem_rpc_id)
    {
      return create_session_st(remote_uri, rem_rpc_id);
    }

    /**
     * @brief Disconnect and destroy a session. The application must not use this
     * session number after this function is called.
     *
     * @param session_num A session number returned from a successful
     * create_session()
     *
     * @return 0 if the session disconnect packet was sent, and the disconnect
     * callback will be invoked later. Negative errno if the session cannot be
     * disconnected.
     */
    int destroy_session(int session_num)
    {
      return destroy_session_st(session_num);
    }

    /**
     * @brief Enqueue a request for transmission. This always succeeds. eRPC owns
     * the request and response msgbufs until it invokes the continuation
     * callback. This function is safe to call from background threads (TS).
     *
     * @param session_num The session number to send the request on. This session
     * must be connected.
     *
     * @param req_type The type of the request. The server for this remote
     * procedure call must have a registered handler for this request type.
     *
     * @param req_msgbuf The MsgBuffer containing the request data
     *
     * @param resp_msgbuf The MsgBuffer that will contain the response data when
     * the continuation is invoked. Its allocation size be large enough to
     * accomodate any response for this request.
     *
     * @param cont_func The continuation that will be invoked when this request
     * completes. See erpc_req_func_t.
     *
     * @param tag A tag for this request that will be passed to the application
     * in the continuation callback
     *
     * @param cont_etid The eRPC thread ID of the background thread to run the
     * continuation on. The default value of \p kInvalidBgETid means that the
     * continuation runs in the foreground. This argument is meant only for
     * internal use by eRPC (i.e., user calls must ignore it).
     */
    void enqueue_request(int session_num, uint8_t req_type, MsgBuffer *req_msgbuf,
                         MsgBuffer *resp_msgbuf, erpc_cont_func_t cont_func,
                         void *tag, size_t cont_etid = kInvalidBgETid);

    /**
     * @brief Enqueue a response for transmission at the server. This must
     * be either the request handle's preallocated response buffer or its
     * dynamic response.  See ReqHandle for details about creating the response.
     * On calling this, the application loses ownership of the request and
     * response MsgBuffer. This function is safe to call from background threads
     * (TS).
     *
     * This can be called outside the request handler.
     *
     * @param req_handle The handle passed to the request handler by eRPC
     *
     * @param resp_msgbuf The message buffer containing the response. This must
     * be either the request handle's preallocated response msgbuf or the request
     * handle's dynamic response msgbuf. The preallocated response msgbuf may be
     * used for only responses that fit in one packet, in which case it is the
     * better choice.
     *
     * @note The restriction on resp_msgbuf is inconvenient to the user because
     * they cannot provide an arbitrary application-owned buffer. Unfortunately,
     * supporting this feature will require passing the response MsgBuffer by
     * value instead of reference since eRPC provides no application callback for
     * when the response can be re-used or freed.
     */
    void enqueue_response(ReqHandle *req_handle, MsgBuffer *resp_msgbuf);

    /// Run the event loop for some milliseconds. See Rpc::run_event_loop_once()
    /// for more on eRPC's event loop.
    inline void run_event_loop(size_t timeout_ms)
    {
      run_event_loop_timeout_st(timeout_ms);
    }

    /**
     * @brief Run one iteration of eRPC's event loop. Users must call this
     * periodically to make progress, since the event loop performs most of eRPC's
     * work, including
     *
     * -# Checking for new session handshake requests and responses, and invoking
     *  corresponding callbacks
     * -# Checking for new datapath requests and responses, and invoking
     *  corresponding callbacks
     * -# Scheduling and transmitting datapath packets, and retransmitting lost
     *  packets
     *
     * This call returns immediately when there is no work to be done.
     */
    inline void run_event_loop_once() { run_event_loop_do_one_st(); }

    /// Identical to alloc_msg_buffer(), but throws an exception on failure
    inline MsgBuffer alloc_msg_buffer_or_die(size_t max_data_size)
    {
      MsgBuffer m = alloc_msg_buffer(max_data_size);
      rt_assert(m.buf_ != nullptr);
      return m;
    }

    /// Return the number of active server or client sessions. This function
    /// can be called only from the creator thread.
    size_t num_active_sessions() { return num_active_sessions_st(); }

    /// Return true iff this session is connected. The session must not have
    /// been disconnected.
    bool is_connected(int session_num) const
    {
      return session_vec_[static_cast<size_t>(session_num)]->is_connected();
    }

    /// Return the physical link bandwidth (bytes per second)
    size_t get_bandwidth() const { return transport_->get_bandwidth(); }

    /// Return the number of retransmissions for a connected session
    size_t get_num_re_tx(int session_num) const
    {
      Session *session = session_vec_[static_cast<size_t>(session_num)];
      return session->client_info_.num_re_tx_;
    }

    /// Reset the number of retransmissions for a connected session
    void reset_num_re_tx(int session_num)
    {
      Session *session = session_vec_[static_cast<size_t>(session_num)];
      session->client_info_.num_re_tx_ = 0;
    }

    /// Return the total amount of huge page memory allocated to the user
    inline size_t get_stat_user_alloc_tot()
    {
      huge_alloc_lock_.lock();
      size_t ret = huge_alloc_->get_stat_user_alloc_tot();
      huge_alloc_lock_.unlock();
      return ret;
    }

    /// Return the Timely instance for a connected session. Expert use only.
    Timely *get_timely(int session_num)
    {
      Session *session = session_vec_[static_cast<size_t>(session_num)];
      return &session->client_info_.cc_.timely_;
    }

    /// Return the Timing Wheel for this Rpc. Expert use only.
    TimingWheel *get_wheel() { return wheel_; }

    /// Set this Rpc's optaque context, which is passed to request handlers and
    /// continuations.
    inline void set_context(void *_context)
    {
      rt_assert(context_ == nullptr, "Cannot reset non-null Rpc context");
      context_ = _context;
    }

    /// Change this Rpc's preallocated response message buffer size
    inline void set_pre_resp_msgbuf_size(size_t new_pre_resp_msgbuf_size)
    {
      pre_resp_msgbuf_size_ = new_pre_resp_msgbuf_size;
    }

    /// Retrieve this Rpc's hugepage allocator. For expert use only.
    inline HugeAlloc *get_huge_alloc() const
    {
      rt_assert(nexus_->num_bg_threads_ == 0,
                "Cannot extract allocator because background threads exist.");
      return huge_alloc_;
    }

    /// Return the maximum *data* size in one packet for the (private) transport
    static inline constexpr size_t get_max_data_per_pkt()
    {
      return TTr::kMaxDataPerPkt;
    }

    /// Return the hostname of the remote endpoint for a connected session
    std::string get_remote_hostname(int session_num) const
    {
      return session_vec_[static_cast<size_t>(session_num)]
          ->get_remote_hostname();
    }

    /// Return the maximum number of sessions supported
    static inline constexpr size_t get_max_num_sessions()
    {
      return Transport::kNumRxRingEntries / kSessionCredits;
    }

    /// Return the data size in bytes that can be sent in one request or response
    static inline size_t get_max_msg_size() { return kMaxMsgSize; }

    /// Return the ID of this Rpc object
    inline uint8_t get_rpc_id() const { return rpc_id_; }

    /// Return true iff the caller is running in a background thread
    inline bool in_background() const { return !in_dispatch(); }

    /// Return the eRPC thread ID of the caller
    inline size_t get_etid() const { return tls_registry_->get_etid(); }

    /// Return RDTSC frequency in GHz
    inline double get_freq_ghz() const { return freq_ghz_; }

    /// Return the number of seconds elapsed since this Rpc was created
    double sec_since_creation()
    {
      return to_sec(rdtsc() - creation_tsc_, freq_ghz_);
    }

    /// Return the average number of packets received in a call to rx_burst
    double get_avg_rx_batch()
    {
      if (!kDatapathStats || dpath_stats_.rx_burst_calls_ == 0)
        return -1.0;
      return dpath_stats_.pkts_rx_ * 1.0 / dpath_stats_.rx_burst_calls_;
    }

    /// Return the average number of packets sent in a call to tx_burst
    double get_avg_tx_batch()
    {
      if (!kDatapathStats || dpath_stats_.tx_burst_calls_ == 0)
        return -1.0;
      return dpath_stats_.pkts_tx_ * 1.0 / dpath_stats_.tx_burst_calls_;
    }

    /// Reset all datapath stats to zero
    void reset_dpath_stats()
    {
      memset(reinterpret_cast<void *>(&dpath_stats_), 0, sizeof(dpath_stats_));
    }

    /**
     * @brief Inject a fault that always fails all routing info resolution
     * @throw runtime_error if the caller cannot inject faults
     */
    void fault_inject_fail_resolve_rinfo_st();

    /**
     * @brief Set the TX packet drop probability for this Rpc
     * @throw runtime_error if the caller cannot inject faults
     */
    void fault_inject_set_pkt_drop_prob_st(double pkt_drop_prob);

    inline bool is_session_not_full(int session_num) const
    {
      //        if(unlikely(session_num<0|| session_num>=session_vec_.size()||session_vec_[session_num]== nullptr)) {
      //            return false;
      //        }
      if (unlikely(session_vec_[session_num] == nullptr))
      {
        return true;
      }
      return !session_vec_[session_num]->is_full();
    }

  private:
    int create_session_st(std::string remote_uri, uint8_t rem_rpc_id);
    int destroy_session_st(int session_num);
    size_t num_active_sessions_st();

    //
    // Session management helper functions
    //

    /// Process all session management packets in the hook's RX list
    void handle_sm_rx_st();

    /// Free a session's resources and mark it as null in the session vector.
    /// Only the MsgBuffers allocated by the Rpc layer are freed. The user is
    /// responsible for freeing user-allocated MsgBuffers.
    void bury_session_st(Session *);

    /// Send an SM packet. The packet's destination (i.e., client or server) is
    /// determined using the packet's type.
    void sm_pkt_udp_tx_st(const SmPkt &);

    /// Send a session management request for a client session. This includes
    /// saving retransmission information for the request. The SM request type is
    /// computed using the session state
    void send_sm_req_st(Session *);

    //
    // Session management packet handlers
    //
    void handle_connect_req_st(const SmPkt &);
    void handle_connect_resp_st(const SmPkt &);

    void handle_disconnect_req_st(const SmPkt &);
    void handle_disconnect_resp_st(const SmPkt &);

    /// Try to reset a client session. If this is not currently possible, the
    /// session state must be set to reset-in-progress.
    bool handle_reset_client_st(Session *session);

    /// Try to reset a server session. If this is not currently possible, the
    /// session state must be set to reset-in-progress.
    bool handle_reset_server_st(Session *session);

    //
    // Methods to bury server-side request and response MsgBuffers. Client-side
    // request and response MsgBuffers are owned by user apps, so eRPC doesn't
    // free their backing memory.
    //

    /**
     * @brief Bury a server sslot's response MsgBuffer (i.e., sslot->tx_msgbuf).
     * This is done in the foreground thread after receiving a packet for the
     * next request.
     *
     * This does not fully validate the MsgBuffer, since we don't want to
     * conditionally bury only initialized MsgBuffers.
     */
    inline void bury_resp_msgbuf_server_st(SSlot *sslot)
    {
      assert(in_dispatch());

      // Free the response MsgBuffer iff it's the dynamically allocated response.
      // This high-specificity checks prevents freeing a null tx_msgbuf.
      if (sslot->tx_msgbuf_ == &sslot->dyn_resp_msgbuf_)
      {
        MsgBuffer *tx_msgbuf = sslot->tx_msgbuf_;
        free_msg_buffer(*tx_msgbuf);
        // Need not nullify tx_msgbuf->buffer.buf: we'll just nullify tx_msgbuf
      }

      sslot->tx_msgbuf_ = nullptr;
    }

    /**
     * @brief Bury a server sslot's request MsgBuffer. This is done in
     * enqueue_response(), so only in the foreground thread.
     *
     * This does not fully validate the MsgBuffer, since we don't want to
     * conditinally bury only initialized MsgBuffers.
     */
    inline void bury_req_msgbuf_server_st(SSlot *sslot)
    {
      MsgBuffer &req_msgbuf = sslot->server_info_.req_msgbuf_;
      if (unlikely(req_msgbuf.is_dynamic()))
      {
        free_msg_buffer(req_msgbuf);
        req_msgbuf.buffer_.buf_ = nullptr; // Mark invalid for future
      }

      req_msgbuf.buf_ = nullptr;
    }

    //
    // Handle available ring entries
    //

    /// Return true iff there are sufficient ring entries available for a session
    bool have_ring_entries() const
    {
      return ring_entries_available_ >= kSessionCredits;
    }

    /// Allocate ring entries for one session
    void alloc_ring_entries()
    {
      assert(have_ring_entries());
      ring_entries_available_ -= kSessionCredits;
    }

    /// Free ring entries allocated for one session
    void free_ring_entries()
    {
      ring_entries_available_ += kSessionCredits;
      assert(ring_entries_available_ <= Transport::kNumRxRingEntries);
    }

    //
    // Datapath helpers
    //

    /// Convert a response packet's wire protocol packet number (pkt_num) to its
    /// index in the response MsgBuffer
    static inline size_t resp_ntoi(size_t pkt_num, size_t num_req_pkts)
    {
      return pkt_num - (num_req_pkts - 1);
    }

    /// Return true iff a packet received by a client is in order. This must be
    /// only a few instructions.
    inline size_t in_order_client(const SSlot *sslot, const pkthdr_t *pkthdr)
    {
      // Counters for pkthdr's request number are valid only if req numbers match
      if (unlikely(pkthdr->req_num_ != sslot->cur_req_num_))
        return false;

      const auto &ci = sslot->client_info_;
      if (unlikely(pkthdr->pkt_num_ != ci.num_rx_))
        return false;

      // Ignore spurious packets received as a consequence of rollback:
      // 1. We've only sent pkts up to (ci.num_tx - 1). Ignore later packets.
      // 2. Ignore if the corresponding client packet for pkthdr is still in wheel
      if (unlikely(pkthdr->pkt_num_ >= ci.num_tx_))
        return false;

      if (kCcPacing &&
          unlikely(ci.in_wheel_[pkthdr->pkt_num_ % kSessionCredits]))
      {
        pkt_loss_stats_.still_in_wheel_during_retx_++;
        return false;
      }

      return true;
    }

    /**
     * @brief Return the number of packets required for \p data_size data bytes.
     *
     * This should avoid division if \p data_size fits in one packet.
     * For \p data_size = 0, the return value need not be 0, i.e., it can be 1.
     */
    static size_t data_size_to_num_pkts(size_t data_size)
    {
      if (data_size <= TTr::kMaxDataPerPkt)
        return 1;
      return (data_size + TTr::kMaxDataPerPkt - 1) / TTr::kMaxDataPerPkt;
    }

    /// Return the total number of packets sent on the wire by one RPC endpoint.
    /// The client must have received the first response packet to call this.
    static inline size_t wire_pkts(MsgBuffer *req_msgbuf,
                                   MsgBuffer *resp_msgbuf)
    {
      return req_msgbuf->num_pkts_ + resp_msgbuf->num_pkts_ - 1;
    }

    /// Return true iff this sslot needs to send more request packets
    static inline bool req_pkts_pending(SSlot *sslot)
    {
      return sslot->client_info_.num_tx_ < sslot->tx_msgbuf_->num_pkts_;
    }

    /// Return true iff it's currently OK to bypass the wheel for this request
    inline bool can_bypass_wheel(SSlot *sslot) const
    {
      if (!kCcPacing)
        return true;
      if (kTesting)
        return faults_.hard_wheel_bypass_;
      if (kCcOptWheelBypass)
      {
        // To prevent reordering, do not bypass the wheel if it contains packets
        // for this session.
        return sslot->client_info_.wheel_count_ == 0 &&
               sslot->session_->is_uncongested();
      }
      return false;
    }

    /// Complete transmission for all packets in the Rpc's TX batch and the
    /// transport's DMA queue
    void drain_tx_batch_and_dma_queue()
    {
      if (tx_batch_i_ > 0)
        do_tx_burst_st();
      transport_->tx_flush();
    }

    /// Add an RPC slot to the list of active RPCs
    inline void add_to_active_rpc_list(SSlot &sslot)
    {
      SSlot *prev_tail = active_rpcs_tail_sentinel_.client_info_.prev_;

      prev_tail->client_info_.next_ = &sslot;
      sslot.client_info_.prev_ = prev_tail;

      sslot.client_info_.next_ = &active_rpcs_tail_sentinel_;
      active_rpcs_tail_sentinel_.client_info_.prev_ = &sslot;
    }

    /// Delete an active RPC slot from the list of active RPCs
    inline void delete_from_active_rpc_list(SSlot &sslot)
    {
      sslot.client_info_.prev_->client_info_.next_ = sslot.client_info_.next_;
      sslot.client_info_.next_->client_info_.prev_ = sslot.client_info_.prev_;
    }

    //
    // Datapath processing
    //

    /// Implementation of the run_event_loop(timeout) API function
    void run_event_loop_timeout_st(size_t timeout_ms);

    /// Actually run one iteration of the event loop
    void run_event_loop_do_one_st();

    /// Enqueue client packets for a sslot that has at least one credit and
    /// request packets to send. Packets may be added to the timing wheel or the
    /// TX burst; credits are used in both cases.
    void kick_req_st(SSlot *);

    /// Enqueue client packets for a sslot that has at least one credit and
    /// RFR packets to send. Packets may be added to the timing wheel or the
    /// TX burst; credits are used in both cases.
    void kick_rfr_st(SSlot *);

    /// Process a single-packet request message. Using (const pkthdr_t *) instead
    /// of (pkthdr_t *) is messy because of fake MsgBuffer constructor.
    void process_small_req_st(SSlot *, pkthdr_t *);

    /// Process a packet for a multi-packet request
    void process_large_req_one_st(SSlot *, const pkthdr_t *);

    /**
     * @brief Process a single-packet response
     * @param rx_tsc The timestamp at which this packet was received
     */
    void process_resp_one_st(SSlot *, const pkthdr_t *, size_t rx_tsc);

    /**
     * @brief Enqueue an explicit credit return
     *
     * @param sslot The session slot to send the explicit CR for
     * @param req_pkthdr The packet header of the request packet that triggered
     * this explicit CR. The packet number of req_pkthdr is copied to the CR.
     */
    void enqueue_cr_st(SSlot *sslot, const pkthdr_t *req_pkthdr);

    /**
     * @brief Process an explicit credit return packet
     * @param rx_tsc Timestamp at which the packet was received
     */
    void process_expl_cr_st(SSlot *, const pkthdr_t *, size_t rx_tsc);

    /**
     * @brief Enqueue a request-for-response. This doesn't modify credits or
     * sslot's num_tx.
     *
     * @param sslot The session slot to send the RFR for
     * @param req_pkthdr The packet header of the response packet that triggered
     * this RFR. Since one response packet can trigger multiple RFRs, the RFR's
     * packet number should be computed from num_tx, not from resp_pkthdr.
     */
    void enqueue_rfr_st(SSlot *sslot, const pkthdr_t *resp_pkthdr);

    /// Process a request-for-response
    void process_rfr_st(SSlot *, const pkthdr_t *);

    /**
     * @brief Enqueue a data packet from sslot's tx_msgbuf for tx_burst
     * @param pkt_idx The index of the packet in tx_msgbuf, not packet number
     */
    inline void enqueue_pkt_tx_burst_st(SSlot *sslot, size_t pkt_idx,
                                        size_t *tx_ts)
    {
      assert(in_dispatch());
      const MsgBuffer *tx_msgbuf = sslot->tx_msgbuf_;

      Transport::tx_burst_item_t &item = tx_burst_arr_[tx_batch_i_];
      item.routing_info_ = sslot->session_->remote_routing_info_;
      item.msg_buffer_ = const_cast<MsgBuffer *>(tx_msgbuf);
      item.pkt_idx_ = pkt_idx;
      if (kCcRTT)
        item.tx_ts_ = tx_ts;

      if (kTesting)
      {
        item.drop_ = roll_pkt_drop();
        testing_.pkthdr_tx_queue_.push(*tx_msgbuf->get_pkthdr_n(pkt_idx));
      }

      ERPC_TRACE("Rpc %u, lsn %u (%s): TX %s. Slot %s.%s\n", rpc_id_,
                 sslot->session_->local_session_num_,
                 sslot->session_->get_remote_hostname().c_str(),
                 tx_msgbuf->get_pkthdr_str(pkt_idx).c_str(),
                 sslot->progress_str().c_str(), item.drop_ ? " Drop." : "");

      tx_batch_i_++;
      if (tx_batch_i_ == TTr::kPostlist)
        do_tx_burst_st();
    }

    /// Enqueue a control packet for tx_burst. ctrl_msgbuf can be reused after
    /// (2 * unsig_batch) calls to this function.
    inline void enqueue_hdr_tx_burst_st(SSlot *sslot, MsgBuffer *ctrl_msgbuf,
                                        size_t *tx_ts)
    {
      assert(in_dispatch());

      Transport::tx_burst_item_t &item = tx_burst_arr_[tx_batch_i_];
      item.routing_info_ = sslot->session_->remote_routing_info_;
      item.msg_buffer_ = ctrl_msgbuf;
      item.pkt_idx_ = 0;
      if (kCcRTT)
        item.tx_ts_ = tx_ts;

      if (kTesting)
      {
        item.drop_ = roll_pkt_drop();
        testing_.pkthdr_tx_queue_.push(*ctrl_msgbuf->get_pkthdr_0());
      }

      ERPC_TRACE("Rpc %u, lsn %u (%s): TX %s. Slot %s.%s.\n", rpc_id_,
                 sslot->session_->local_session_num_,
                 sslot->session_->get_remote_hostname().c_str(),
                 ctrl_msgbuf->get_pkthdr_str(0).c_str(),
                 sslot->progress_str().c_str(), item.drop_ ? " Drop." : "");

      tx_batch_i_++;
      if (tx_batch_i_ == TTr::kPostlist)
        do_tx_burst_st();
    }

    /// Enqueue a request packet to the timing wheel
    inline void enqueue_wheel_req_st(SSlot *sslot, size_t pkt_num)
    {
      const size_t pkt_idx = pkt_num;
      size_t pktsz =
          sslot->tx_msgbuf_->get_pkt_size<TTr::kMaxDataPerPkt>(pkt_idx);
      size_t ref_tsc = dpath_rdtsc();
      size_t desired_tx_tsc =
          sslot->session_->cc_getupdate_tx_tsc(ref_tsc, pktsz);

      ERPC_CC("Rpc %u: lsn/req/pkt %u/%zu/%zu, REQ wheeled for %.3f us.\n",
              rpc_id_, sslot->session_->local_session_num_, sslot->cur_req_num_,
              pkt_num, to_usec(desired_tx_tsc - creation_tsc_, freq_ghz_));

      wheel_->insert(wheel_ent_t(sslot, pkt_num), ref_tsc, desired_tx_tsc);
      sslot->client_info_.in_wheel_[pkt_num % kSessionCredits] = true;
      sslot->client_info_.wheel_count_++;
    }

    /// Enqueue an RFR packet to the timing wheel
    inline void enqueue_wheel_rfr_st(SSlot *sslot, size_t pkt_num)
    {
      const size_t pkt_idx = resp_ntoi(pkt_num, sslot->tx_msgbuf_->num_pkts_);
      const MsgBuffer *resp_msgbuf = sslot->client_info_.resp_msgbuf_;
      size_t pktsz = resp_msgbuf->get_pkt_size<TTr::kMaxDataPerPkt>(pkt_idx);
      size_t ref_tsc = dpath_rdtsc();
      size_t desired_tx_tsc =
          sslot->session_->cc_getupdate_tx_tsc(ref_tsc, pktsz);

      ERPC_CC("Rpc %u: lsn/req/pkt %u/%zu/%zu, RFR wheeled for %.3f us.\n",
              rpc_id_, sslot->session_->local_session_num_, sslot->cur_req_num_,
              pkt_num, to_usec(desired_tx_tsc - creation_tsc_, freq_ghz_));

      wheel_->insert(wheel_ent_t(sslot, pkt_num), ref_tsc, desired_tx_tsc);
      sslot->client_info_.in_wheel_[pkt_num % kSessionCredits] = true;
      sslot->client_info_.wheel_count_++;
    }

    /// Transmit packets in the TX batch
    inline void do_tx_burst_st()
    {
      assert(in_dispatch());
      assert(tx_batch_i_ > 0);

      // Measure TX burst size
      dpath_stat_inc(dpath_stats_.tx_burst_calls_, 1);
      dpath_stat_inc(dpath_stats_.pkts_tx_, tx_batch_i_);

      if (kCcRTT)
      {
        size_t batch_tsc = 0;
        if (kCcOptBatchTsc)
          batch_tsc = dpath_rdtsc();

        for (size_t i = 0; i < tx_batch_i_; i++)
        {
          if (tx_burst_arr_[i].tx_ts_ != nullptr)
          {
            *tx_burst_arr_[i].tx_ts_ = kCcOptBatchTsc ? batch_tsc : dpath_rdtsc();
          }
        }
      }

      transport_->tx_burst(tx_burst_arr_, tx_batch_i_);
      tx_batch_i_ = 0;
    }

    /// Return a credit to this session
    static inline void bump_credits(Session *session)
    {
      assert(session->is_client());
      assert(session->client_info_.credits_ < kSessionCredits);
      session->client_info_.credits_++;
    }

    /// Copy the data from a packet to a MsgBuffer at a packet index
    static inline void copy_data_to_msgbuf(MsgBuffer *msgbuf, size_t pkt_idx,
                                           const pkthdr_t *pkthdr)
    {
      size_t offset = pkt_idx * TTr::kMaxDataPerPkt;
      size_t to_copy =
          (std::min)(TTr::kMaxDataPerPkt, pkthdr->msg_size_ - offset);
      memcpy(&msgbuf->buf_[offset], pkthdr + 1, to_copy); // From end of pkthdr
    }

    /**
     * @brief Process received packets and post RECVs. The ring buffers received
     * from `rx_burst` must not be used after new RECVs are posted.
     *
     * Although none of the polled RX ring buffers can be overwritten by the
     * NIC until we send at least one response/CR packet back, we do not control
     * the order or time at which these packets are sent, due to constraints like
     * session credits and packet pacing.
     */
    void process_comps_st();

    /**
     * @brief Submit a request work item to a random background thread
     *
     * @param sslot Session sslot with a complete request. Used only for request
     * work item types.
     */
    void submit_bg_req_st(SSlot *sslot);

    /**
     * @brief Submit a response work item to a specific background thread
     *
     * @param cont_func The continuation to invoke
     *
     * @param tag The tag of the completed request. Used only for response work
     * item types.
     *
     * @param bg_etid eRPC thread ID of the background thread to submit to
     */
    void submit_bg_resp_st(erpc_cont_func_t cont_func, void *tag, size_t bg_etid);

    //
    // Queue handlers
    //

    /// Try to transmit request packets from sslots that are stalled for credits.
    void process_credit_stall_queue_st();

    /// Process the wheel. We have already paid credits for sslots in the wheel.
    void process_wheel_st();

    /// Process the requests enqueued by background threads
    void process_bg_queues_enqueue_request_st();

    /// Process the responses enqueued by background threads
    void process_bg_queues_enqueue_response_st();

    /**
     * @brief Check if the caller can inject faults
     * @throw runtime_error if the caller cannot inject faults
     */
    void fault_inject_check_ok() const;

    //
    // Packet loss handling
    //

    /// Scan sessions and requests for session management and datapath packet loss
    void pkt_loss_scan_st();

    /// Retransmit packets for an sslot for which we suspect a packet loss
    void pkt_loss_retransmit_st(SSlot *sslot);

    //
    // Misc private functions
    //

    /// Return true iff we're currently running in this Rpc's creator thread
    // modified because we don't use background threads
    inline bool in_dispatch() const { return true; }

    /// Return true iff a user-provided session number is in the session vector
    inline bool is_usr_session_num_in_range_st(int session_num) const
    {
      assert(in_dispatch());
      return session_num >= 0 &&
             static_cast<size_t>(session_num) < session_vec_.size();
    }

    /// Lock the mutex if the Rpc is accessible from multiple threads
    // inline void lock_cond(std::mutex *mutex)
    // {
    //   if (unlikely(multi_threaded_))
    //     mutex->lock();
    // }

    /// Unlock the mutex if the Rpc is accessible from multiple threads
    // inline void unlock_cond(std::mutex *mutex)
    // {
    //   if (unlikely(multi_threaded_))
    //     mutex->unlock();
    // }

    /**
     * @brief Perform a Timely rate update on receiving the explict CR or response
     * packet for this triggering packet number
     *
     * @param sslot The request sslot for which a packet is received
     * @param pkt_num The received packet's packet number
     * @param Time at which the explicit CR or response packet was received
     */
    inline void update_timely_rate(SSlot *sslot, size_t pkt_num, size_t rx_tsc)
    {
      size_t rtt_tsc =
          rx_tsc - sslot->client_info_.tx_ts_[pkt_num % kSessionCredits];
      // This might use Timely bypass
      sslot->session_->client_info_.cc_.timely_.update_rate(rx_tsc, rtt_tsc);
    }

    /// Return true iff a packet should be dropped
    inline bool roll_pkt_drop()
    {
      static constexpr uint32_t kBillion = 1000000000;
      return ((fast_rand_.next_u32() % kBillion) <
              faults_.pkt_drop_thresh_billion_);
    }

  public:
    // Hooks for apps to modify eRPC behavior

    /// Retry session connection if the remote RPC ID was invalid. This usually
    /// happens when the server RPC thread has not started.
    bool retry_connect_on_invalid_rpc_id_ = false;

  private:
    // Constructor args
    Nexus *nexus_;
    void *context_; ///< The application context
    const uint8_t rpc_id_;
    const sm_handler_t sm_handler_;
    const uint8_t phy_port_; ///< Zero-based physical port specified by app
    const size_t numa_node_;

    // Derived
    const size_t creation_tsc_;             ///< Timestamp of creation of this Rpc endpoint
    const bool multi_threaded_;             ///< True iff there are background threads
    const double freq_ghz_;                 ///< RDTSC frequency, derived from Nexus
    const size_t rpc_rto_cycles_;           ///< RPC RTO in cycles
    const size_t rpc_pkt_loss_scan_cycles_; ///< Packet loss scan frequency

    /// A copy of the request/response handlers from the Nexus. We could use
    /// a pointer instead, but an array is faster.
    const std::array<ReqFunc, kReqTypeArraySize> req_func_arr_;

    // Rpc metadata
    size_t creator_etid_;       ///< eRPC thread ID of the creator thread
    TlsRegistry *tls_registry_; ///< Pointer to the Nexus's thread-local registry

    // Sessions

    /// The append-only list of session pointers, indexed by session number.
    /// Disconnected sessions are denoted by null pointers. This grows as sessions
    /// are repeatedly connected and disconnected, but 8 bytes per session is OK.
    std::vector<Session *> session_vec_;

    // Transport
    TTr *transport_ = nullptr; ///< The unreliable transport

    /// Current number of ring buffers available to use for sessions
    size_t ring_entries_available_ = TTr::kNumRxRingEntries;

    Transport::tx_burst_item_t tx_burst_arr_[TTr::kPostlist]; ///< Tx batch info
    size_t tx_batch_i_ = 0;                                   ///< The batch index for TX burst array

    /// On calling rx_burst(), Transport fills-in packet buffer pointers into the
    /// RX ring. Some transports such as InfiniBand and Raw reuse RX ring packet
    /// buffers in a circular order, so the ring's pointers remain unchanged
    /// after initialization. Other transports (e.g., DPDK) update rx_ring on
    /// every successful rx_burst.
    uint8_t *rx_ring_[TTr::kNumRxRingEntries];
    size_t rx_ring_head_ = 0; ///< Current unused RX ring buffer

    std::vector<SSlot *> stallq_; ///< Request sslots stalled for credits

    size_t ev_loop_tsc_; ///< TSC taken at each iteration of the ev loop

    // Packet loss
    size_t pkt_loss_scan_tsc_; ///< Timestamp of the previous scan for lost pkts

    /// The doubly-linked list of active RPCs. An RPC slot is added to this list
    /// when the request is enqueued. The slot is deleted from this list when its
    /// continuation is invoked or queued to a background thread.
    ///
    /// This should not be a vector because we need random deletes. Having
    /// permanent root and tail sentinels allows adding and deleting slots from
    /// the list without conditionals.
    SSlot active_rpcs_root_sentinel_, active_rpcs_tail_sentinel_;

    // Allocator
    HugeAlloc *huge_alloc_ = nullptr; ///< This thread's hugepage allocator
    spin_lock_mutex huge_alloc_lock_; ///< A lock to guard the huge allocator

    MsgBuffer ctrl_msgbufs_[2 * TTr::kUnsigBatch]; ///< Buffers for RFR/CR
    size_t ctrl_msgbuf_head_ = 0;
    FastRand fast_rand_; ///< A fast random generator

    // Cold members live below, in order of coolness

    /// The timing-wheel rate limiter. Packets in the wheel have consumed credits,
    /// but not bumped the num_tx counter.
    TimingWheel *wheel_;

    /// Queues for datapath API requests from background threads
    struct
    {
      MtQueue<enq_req_args_t> enqueue_request_;
      MtQueue<enq_resp_args_t> enqueue_response_;
    } bg_queues_;

    // Misc
    SlowRand slow_rand_;          ///< A slow random generator for "real" randomness
    UDPClient<SmPkt> udp_client_; ///< UDP endpoint used to send SM packets
    Nexus::Hook nexus_hook_;      ///< A hook shared with the Nexus

    /// To avoid allocating a new session on receiving a duplicate session
    /// connect request, the server remembers all (XXX) the unique connect
    /// requests it has received. To accomplish this, the client generates a
    /// globally-unique (XXX) token for the first copy of its connect request.
    /// The server saves maps this token to the index of the allocated session.
    std::map<conn_req_uniq_token_t, uint16_t> conn_req_token_map_;

    /// Sessions for which a session management request is outstanding
    std::set<uint16_t> sm_pending_reqs_;

    /// All the faults that can be injected into eRPC for testing
    struct
    {
      bool fail_resolve_rinfo_ = false; ///< Fail routing info resolution
      bool hard_wheel_bypass_ = false;  ///< Wheel bypass regardless of congestion
      double pkt_drop_prob_ = 0.0;      ///< Probability of dropping an RPC packet

      /// Derived: Drop packet iff urand[0, ..., one billion] is smaller than this
      uint32_t pkt_drop_thresh_billion_ = 0;
    } faults_;

    // Additional members for testing
    struct
    {
      FixedQueue<pkthdr_t, kSessionCredits> pkthdr_tx_queue_;
    } testing_;

    /// File for dispatch thread trace output. This is used indirectly by
    /// ERPC_TRACE and other macros.
    FILE *trace_file_;

    /// Datapath stats that can be disabled at compile-time
    struct
    {
      size_t ev_loop_calls_ = 0;
      size_t pkts_tx_ = 0;
      size_t tx_burst_calls_ = 0;
      size_t pkts_rx_ = 0;
      size_t rx_burst_calls_ = 0;
    } dpath_stats_;

  public:
    struct
    {
      size_t num_re_tx_ = 0; /// Total retransmissions across all sessions

      /// Number of times we could not retransmit a request, or we had to drop
      /// a received packet, because a request reference was still in the wheel.
      size_t still_in_wheel_during_retx_ = 0;
    } pkt_loss_stats_;

    /// Size of the preallocated response buffer. This is one packet by default,
    /// but some applications might benefit from a larger preallocated buffer,
    /// at the expense of increased memory utilization.
    size_t pre_resp_msgbuf_size_ = TTr::kMaxDataPerPkt;
  };

// This goes at the end of every Rpc implementation file to force compilation
#define FORCE_COMPILE_TRANSPORTS template class Rpc<CTransport>;
} // namespace erpc
