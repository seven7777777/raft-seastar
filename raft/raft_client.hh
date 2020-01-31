#pragma once

#include "raft/raft_interface.hh"

namespace laomd {
namespace raft {
class RaftClient : public Raft {
  rpc_client client_;
  ms_t timeout_;

public:
  RaftClient(seastar::rpc::client_options opts, const seastar::ipv4_addr &addr,
             ms_t time_out)
      : client_(opts, addr), timeout_(time_out) {}
  virtual ~RaftClient() = default;

  virtual void start() override {}
  virtual seastar::future<> stop() override { return client_.stop(); }

  virtual void on_register(rpc_protocol &proto,
                           uint64_t rpc_verb_base) override {
    proto.register_handler(rpc_verb_base + 1, [](term_t term, id_t candidateId,
                                                 term_t llt, size_t lli) {
      return seastar::make_ready_future<term_t, id_t, bool>(0, 0, false);
    });
    proto.register_handler(rpc_verb_base + 3, [] {
      return seastar::make_ready_future<term_t, id_t, bool>(0, 0, false);
    });
  }

  virtual seastar::future<term_t, id_t, bool>
  RequestVote(term_t term, id_t candidateId, term_t llt, size_t lli) override {
    auto func = client_.get_handler<seastar::future<term_t, id_t, bool>(
        term_t, id_t, term_t, size_t)>(*this, 1);
    return func(client_, timeout_, term, candidateId, llt, lli);
  }

  // return currentTerm, serverId and whether is leader
  virtual seastar::future<term_t, id_t, bool> GetState() override {
    auto func =
        client_.get_handler<seastar::future<term_t, id_t, bool>()>(*this, 3);
    return func(client_, timeout_);
  }
};

} // namespace raft
} // namespace laomd