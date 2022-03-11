
#pragma once

namespace sandbox{
    void install_seccomp();
    void no_fd_leak();
    void drop_root();
}
