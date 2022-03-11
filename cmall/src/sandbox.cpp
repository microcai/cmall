
#include "sandbox.hpp"

#ifdef __linux__
void sandbox::install_seccomp()
{
}

void sandbox::drop_root()
{
}

#else

void sandbox::install_seccomp()
{
}

void sandbox::drop_root()
{
}

#endif
