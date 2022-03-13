
#pragma once

#include <boost/asio/awaitable.hpp>

namespace sandbox{
    void install_seccomp(int notifyfd);

    boost::asio::awaitable<void> seccomp_supervisor(int seccomp_notify_fd_);

    void no_fd_leak();
    void drop_root();
}
