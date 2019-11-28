#pragma once
#include "lao_utils/function.hh"
#include "smf/log.h"
#include "smf/rpc_client.h"
#include "smf/rpc_server.h"
#include <algorithm>
#include <bits/stdint-uintn.h>
#include <boost/test/tools/old/interface.hpp>
#include <functional>
#include <map>
#include <numeric>
#include <raft/raft_impl.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/future.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/sstring.hh>
#include <vector>

namespace laomd {

class config {
  std::map<raft::id_t, seastar::shared_ptr<smf::rpc_server>> servers_;
  std::map<raft::id_t, seastar::shared_ptr<raft::RaftClient>> stubs_;
  const raft::ms_t electionTimeout_;

  static std::vector<uint16_t> get_available_ports(int n) {
    std::vector<uint16_t> ports(n);
    for (auto& p: ports) {
      p = rand() % 100 + 20000; 
    }
    return ports;
  }

public:
  config(int n, raft::ms_t electionTimeout, raft::ms_t heartbeartInterval)
      : electionTimeout_(electionTimeout) {
    using namespace std::chrono;
    auto ports = get_available_ports(n);
    for (auto p : ports) {
      smf::rpc_server_args args;
      args.rpc_port = p;
      args.flags |= smf::rpc_server_flags_disable_http_server;
      auto server = seastar::make_shared<smf::rpc_server>(args);
      std::vector<seastar::ipv4_addr> others(n - 1);
      std::copy_if(ports.begin(), ports.end(), others.begin(),
                   std::bind1st(std::not_equal_to<uint16_t>(), p));
      server->register_service<raft::RaftImpl>(p, others, electionTimeout, heartbeartInterval);
      servers_[p] = server;

      smf::rpc_client_opts opts;
      opts.server_addr = p;
      stubs_[p] = seastar::make_shared<raft::RaftClient>(opts);
    }
    for (auto &&s : servers_) {
      s.second->start();
    }
  }

  seastar::future<raft::id_t> checkOneLeader() {
    using namespace std::chrono;
    for (;;) {
      co_await seastar::sleep(electionTimeout_);
      std::map<raft::term_t, raft::id_t> leaders;
      for (auto &&item : stubs_) {
        auto stub = item.second;
        auto fut =
            stub->connect()
                .then([this, stub, &leaders] {
                  smf::rpc_typed_envelope<raft::GetStateReq> req;
                  return stub->GetState(req.serialize_data())
                      .then([this, stub, &leaders](auto &&rsp) {
                        if (rsp->isLeader()) {
                          auto term = rsp->term();
                          LOG_THROW_IF(leaders.find(term) != leaders.end(),
                                       "term has a server {}!={}", term,
                                       leaders[term], rsp->serverId());
                          leaders[rsp->term()] = rsp->serverId();
                        }
                      });
                }) // connection refused error
                .handle_exception_type(ignore_exception<std::system_error>)
                .handle_exception_type(ignore_exception<smf::remote_connection_error>)
                .handle_exception([this] (auto e) {
                  LOG_WARN("unexpected exception {}", e);
                });
        co_await with_timeout(100ms, std::move(fut));
      }
      if (!leaders.empty()) {
        co_return leaders.begin()->second;
      }
    }
    LOG_THROW("there was no leader elected");
  }

  seastar::sstring available_servers() const {
    seastar::sstring res;
    for (auto [id, server] : servers_) {
      res += seastar::to_sstring(id) + " ";
    }
    return res;
  }

  seastar::future<> stop(raft::id_t id) {
    auto it = servers_.find(id);
    LOG_THROW_IF(it == servers_.end(), "cannot find server {}, available {}",
                 id, available_servers());
    auto it2 = stubs_.find(id);
    return seastar::when_all_succeed(it->second->stop(), it2->second->stop()).then([it, it2, this] {
      servers_.erase(it);
      stubs_.erase(it2);
    });
  }

  void start(raft::id_t id) {
    // auto it = servers_.find(id);
    // LOG_THROW_IF(it == servers_.end(), "cannot find server {}, available {}",
    //              id, available_servers());
    // return it->second->start();
  }

  seastar::future<> clean_up() {
    std::vector<seastar::future<>> futs;
    for (auto &&s : stubs_) {       
      futs.emplace_back(s.second->stop());     
    }
    for (auto &&s : servers_) {
      futs.emplace_back(s.second->stop());
    }
    return seastar::when_all_succeed(futs.begin(), futs.end()).then([this] {
      servers_.clear();
      stubs_.clear();
    });
  }
};

} // namespace laomd