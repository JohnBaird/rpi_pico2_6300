#pragma once

#include "config/RuntimeConfig.h"

namespace network {

class EthernetManager {
  public:
    EthernetManager();

    bool init(const config::RuntimeConfig& runtime_config);
    void service();
    bool status() const;
    const char* last_error() const;
    void handle_dhcp_assigned();
    void handle_dhcp_conflict();

  private:
    bool configure_network(const config::RuntimeConfig& runtime_config);
    bool configure_static_network(const config::RuntimeConfig& runtime_config);
    bool run_dhcp_until_leased();
    void print_active_ip_address() const;

    bool initialized_;
    bool dhcp_enabled_;
    bool dhcp_leased_;
    bool dhcp_conflict_;
    const char* last_error_;
    unsigned int last_dhcp_run_ms_;
};

}  // namespace network
