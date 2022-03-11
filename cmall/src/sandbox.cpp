
#include "sandbox.hpp"

#ifdef __linux__
void sandbox::install_seccomp()
{
}

void sandbox::drop_root()
{
}

void sandbox::no_fd_leak()
{
    
}

#else

void sandbox::install_seccomp()
{
}

void sandbox::drop_root()
{
}

void sandbox::no_fd_leak()
{
    
}
#endif
