#ifndef SRC_NODE_QUIC_SOCKET_H_
#define SRC_NODE_QUIC_SOCKET_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_crypto.h"  // SSLWrap
#include "ngtcp2/ngtcp2.h"
#include "node.h"
#include "node_quic_session.h"
#include "node_quic_util.h"
#include "env.h"
#include "handle_wrap.h"
#include "v8.h"
#include "uv.h"

#include <map>
#include <string>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace quic {

class QuicSocket : public HandleWrap {
 public:
  static void Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context);

  QuicSocket(
    Environment* env,
    Local<Object> wrap);
  ~QuicSocket() override;

  int AddMembership(
    const char* address,
    const char* iface);
  void AddSession(
    QuicCID& cid,
    QuicSession* session);
  void AssociateCID(
    QuicCID& cid,
    QuicServerSession* session);
  int Bind(
    const char* address,
    uint32_t port,
    uint32_t flags,
    int family);
  void DisassociateCID(
    QuicCID& cid);
  int DropMembership(
    const char* address,
    const char* iface);
  SocketAddress* GetLocalAddress();
  void Listen(
    crypto::SecureContext* context);
  int ReceiveStart();
  int ReceiveStop();
  void RemoveSession(
    QuicCID& cid);
  void ReportSendError(
    int error);
  void SendPendingData(
    bool retransmit = false);
  int SetBroadcast(
    bool on);
  int SetMulticastInterface(
    const char* iface);
  int SetMulticastLoopback(
    bool on);
  int SetMulticastTTL(
    int ttl);
  int SetTTL(
    int ttl);
  int SendPacket(
    SocketAddress* dest,
    QuicBuffer* buf);
  int SendPacket(
    const sockaddr* dest,
    QuicBuffer* buf);
  void SetServerSessionSettings(
    QuicSession* session,
    ngtcp2_settings* settings);

  crypto::SecureContext* GetServerSecureContext() {
    return server_secure_context_;
  }

  const uv_udp_t* operator*() const { return &handle_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicSocket)
  SET_SELF_SIZE(QuicSocket)

 private:
  static void OnAlloc(
      uv_handle_t* handle,
      size_t suggested_size,
      uv_buf_t* buf);

  static void OnRecv(
      uv_udp_t* handle,
      ssize_t nread,
      const uv_buf_t* buf,
      const struct sockaddr* addr,
      unsigned int flags);

  // Receives packets from the uv_udt_t handle as they arrive.
  void Receive(
      ssize_t nread,
      const uv_buf_t* buf,
      const struct sockaddr* addr,
      unsigned int flags);

  int SendVersionNegotiation(
      const ngtcp2_pkt_hd* chd,
      const sockaddr* addr);

  QuicSession* ServerReceive(
      QuicCID& dcid,
      ngtcp2_pkt_hd* hd,
      ssize_t nread,
      const uint8_t* data,
      const struct sockaddr* addr,
      unsigned int flags);
  int SendRetry(
    const ngtcp2_pkt_hd* chd,
    const sockaddr* addr);

  template <typename T,
            int (*F)(const typename T::HandleType*, sockaddr*, int*)>
  friend void node::GetSockOrPeerName(
      const v8::FunctionCallbackInfo<v8::Value>&);

  // Fields and TypeDefs
  typedef uv_udp_t HandleType;

  uv_udp_t handle_;
  SocketAddress local_address_;
  bool server_listening_;
  bool validate_addr_;
  QuicSessionConfig server_session_config_;
  crypto::SecureContext* server_secure_context_;
  std::unordered_map<std::string, QuicSession*> sessions_;
  std::unordered_map<std::string, std::string> dcid_to_scid_;
  CryptoContext token_crypto_ctx_;
  std::array<uint8_t, TOKEN_SECRETLEN> token_secret_;

  struct socket_stats {
    // The total number of bytes received (and not ignored)
    // by this QuicSocket instance.
    uint64_t bytes_received;

    // The total number of bytes successfully sent by this
    // QuicSocket instance.
    uint64_t bytes_sent;

    // The total number of packets received (and not ignored)
    // by this QuicSocket instance.
    uint64_t packets_received;

    // The total number of packets successfully sent by this
    // QuicSocket instance.
    uint64_t packets_sent;

    // The total number of QuicServerSessions that have been
    // associated with this QuicSocket instance.
    uint64_t server_sessions;

    // The total number of QuicClientSessions that have been
    // associated with this QuicSocket instance.
    uint64_t client_sessions;

    // The total number of times packets have had to be
    // retransmitted by this QuicSocket instance.
    uint64_t retransmit_count;
  };
  socket_stats socket_stats_{0,0,0,0,0,0,0};

  template <typename... Members>
  void IncrementSocketStat(uint64_t amount, socket_stats* a, Members... mems) {
    static uint64_t max = std::numeric_limits<uint64_t>::max();
    uint64_t current = access(*a, mems...);
    uint64_t delta = std::min(amount, max - current);
    access(*a, mems...) += delta;
  }

  class SendWrap {
   public:
    SendWrap(
        QuicSocket* socket,
        SocketAddress* dest,
        QuicBuffer* buffer,
        int tries = 5);
    SendWrap(
        QuicSocket* socket,
        const sockaddr* dest,
        QuicBuffer* buffer,
        int tries = 5);

    static void OnSend(
        uv_udp_send_t* req,
        int status);

    int Send();

    bool ShouldRetry(int error);

    QuicSocket* Socket() const { return socket_; }

   private:
    uv_udp_send_t req_;
    QuicSocket* socket_;
    MaybeStackBuffer<char> buf_;
    SocketAddress address_;
    int tries_;
  };
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_SOCKET_H_
