#include "rpc/rpc.hh"

namespace laomd {

LOG_SETUP(rpc_protocol);
LOG_SETUP(rpc_server);

void rpc_server::start() {
  LOG_INFO("starting server at {}", addr_);
  for (auto &service : services_) {
    LOG_INFO("start rpc service {}", service->name());
    service->start();
  }
}

seastar::future<> rpc_server::stop() {
  std::deque<seastar::future<>> futs;
  for (auto &service : services_) {
    LOG_INFO("stop rpc service {}", service->name());
    futs.emplace_back(service->stop());
  }
  return seastar::when_all(futs.begin(), futs.end())
      .then_wrapped(
          [this](auto &&fut) { return rpc_protocol::server::stop(); });
}

} // namespace laomd