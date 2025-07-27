#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <butil/fd_guard.h>
#include <butil/endpoint.h>
#include <butil/memory/singleton_on_pthread_once.h>
#include <glog/logging.h>


namespace common{

class LocalAddress {
  public:
    static LocalAddress *get_instance(){
      static LocalAddress instance;
      return &instance;
    }

    std::string get_str_ip(){
      return str_ip_;
    }

    uint32_t get_int_ip(){
      return int_ip_;
    }

private:
  std::string str_ip_;
  uint32_t int_ip_;

  LocalAddress() {
    std::string remote_host = "www.baidu.com";
    int         remote_port = 80;
    butil::ip_t resolved_ip;
    if (butil::hostname2ip(remote_host.c_str(), &resolved_ip)) {
      LOG(FATAL) << "failed to resolve " << remote_host;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
      LOG(FATAL) << "failed to create socket";
    }

    butil::fd_guard file_guard(fd);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr   = resolved_ip;
    sin.sin_port   = htons(remote_port);
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&sin), sizeof(struct sockaddr_in)) < 0) {
      LOG(FATAL) << "failed to connect to " << remote_host;
    }

    butil::EndPoint endpoint;
    if (butil::get_local_side(fd, &endpoint)) {
      LOG(FATAL) << "failed to get_local_side";
    }

    auto v  = butil::ip2str(endpoint.ip);
    int_ip_ = endpoint.ip.s_addr;
    str_ip_   = v.c_str();
  }
};

}; //namespace common
