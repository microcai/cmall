
#pragma once

#ifdef __linux

#include <string>
#include <map>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

namespace sandbox
{
    void install_seccomp(int notifyfd);

    void no_fd_leak();
    void drop_root();

    // 有一个 sandbox 的 vfs 环境, 可以设定被 sandbox 的 node.exe 的 vfs.
    typedef std::map<std::string, std::string_view> sandboxVFS;

    class supervisor
    {
    public:
        supervisor(boost::asio::posix::stream_descriptor&& fd, sandboxVFS vfs);
        ~supervisor();

        boost::asio::awaitable<void> start_supervisor();

    private:
        boost::asio::posix::stream_descriptor seccomp_notify_fd;
        sandboxVFS vfs;
    };
}

#endif
