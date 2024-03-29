

#include "stdafx.hpp"
#include <boost/asio.hpp>
#include <boost/asio/this_coro.hpp>
#include <iostream>

#include "utils/scoped_exit.hpp"

#include "utils/logging.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;

#ifdef __linux__

#include "sandbox.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/mman.h>
#include <seccomp.h>

static int send_fd(int sock, int fd)
{
	struct msghdr msg = {};
	struct cmsghdr* cmsg;
	char buf[CMSG_SPACE(sizeof(int))] = { 0 }, c = 'c';
	struct iovec io = {
		.iov_base = &c,
		.iov_len  = 1,
	};

	msg.msg_iov				 = &io;
	msg.msg_iovlen			 = 1;
	msg.msg_control			 = buf;
	msg.msg_controllen		 = sizeof(buf);
	cmsg					 = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level		 = SOL_SOCKET;
	cmsg->cmsg_type			 = SCM_RIGHTS;
	cmsg->cmsg_len			 = CMSG_LEN(sizeof(int));
	*((int*)CMSG_DATA(cmsg)) = fd;
	msg.msg_controllen		 = cmsg->cmsg_len;

	if (sendmsg(sock, &msg, 0) < 0)
	{
		perror("sendmsg");
		return -1;
	}

	return 0;
}

void sandbox::install_seccomp(int notifyfd)
{
//	auto seccomp_ctx = seccomp_init(SCMP_ACT_ERRNO(ENOSYS));
	auto seccomp_ctx = seccomp_init(SCMP_ACT_ERRNO(ENOSYS));

	// default syscall that should always success.
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(cacheflush), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(get_robust_list), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(get_thread_area), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgroups), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgroups32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgrp), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getppid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresgid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresgid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresuid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresuid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid32), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(membarrier), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(nanosleep), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pause), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(prlimit64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(restart_syscall), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rseq), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getaffinity), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_yield), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_robust_list), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_thread_area), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_tid_address), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_tls), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigreturn), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(time), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ugetrlimit), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe2), 0);

	// basic-io that should always success
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(_llseek), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(close_range), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup3), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(readv), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(readlink), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(access), 0);

	// epoll/select/poll
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(_newselect), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_create), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_create1), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_ctl), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_ctl_old), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_pwait), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_pwait2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait_old), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(eventfd), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(eventfd2), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pselect6), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(pselect6_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(select), 0);

	// allowed to make outbound socket
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_NOTIFY, SCMP_SYS(connect), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpeername), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockname), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockopt), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(recv), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvfrom), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmmsg), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmmsg_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(send), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmmsg), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmsg), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendto), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(setsockopt), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(shutdown), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(socket), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketcall), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketpair), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(bind), 0);

	// thread is allowed
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rseq), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone3), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(tgkill), 0);

	// some generic kernel service allowed
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ERRNO(ENOENT), SCMP_SYS(getcwd), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getcpu), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpriority), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioprio_get), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(mremap), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_get_priority_max), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_get_priority_min), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getattr), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getparam), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getscheduler), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_rr_get_interval), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_rr_get_interval_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_yield), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(splice), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sysinfo), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(tee), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(umask), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(uname), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(vmsplice), 0);

	// signal allowed
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigpending), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigsuspend), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigtimedwait), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigtimedwait_time64), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigaction), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigaltstack), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(signal), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(signalfd), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(signalfd4), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigpending), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigprocmask), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigsuspend), 0);

	// hack allowd for lib loading
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_NOTIFY, SCMP_SYS(openat), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 0);
	seccomp_rule_add_exact(seccomp_ctx, SCMP_ACT_ALLOW, SCMP_SYS(statx), 0);

	seccomp_load(seccomp_ctx);

	int _seccomp_notify_fd = seccomp_notify_fd(seccomp_ctx);

	send_fd(notifyfd, _seccomp_notify_fd);
	close(_seccomp_notify_fd);
	close(notifyfd);

	// TODO, openat 这个调用, 要写详细的 bpf 代码, 只允许打开系统的库
}

void sandbox::drop_root()
{
	setgid(65536);
	setuid(65536);
}

void sandbox::no_fd_leak() { }

sandbox::supervisor::~supervisor()
{

}

sandbox::supervisor::supervisor(boost::asio::posix::stream_descriptor&& fd, sandboxVFS vfs)
	: seccomp_notify_fd(std::move(fd))
	, vfs(vfs)
{

}

awaitable<void> sandbox::supervisor::start_supervisor()
{
	auto this_executor = co_await boost::asio::this_coro::executor;
	boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;

	if (cs.slot().is_connected())
	{
		cs.slot().assign([this](boost::asio::cancellation_type_t) { seccomp_notify_fd.close(); });
	}

	char path[8192];
	char openat_param1[8193];

	const long page_size = sysconf(_SC_PAGE_SIZE);

	while (true)
	{
		co_await seccomp_notify_fd.async_wait(boost::asio::posix::stream_descriptor::wait_read, use_awaitable);
		pollfd onefd;
		onefd.fd	 = seccomp_notify_fd.native_handle();
		onefd.events = POLL_IN;

		while (::poll(&onefd, 1, 1) == 1)
		{
			seccomp_notif* req		 = nullptr;
			seccomp_notif_resp* resp = nullptr;

			seccomp_notify_alloc(&req, &resp);

			scoped_exit cleanup([=]() { ::seccomp_notify_free(req, resp); });
			auto ret = seccomp_notify_receive(seccomp_notify_fd.native_handle(), req);
			if (ret != 0)
			{
				int e = errno;
				LOG_DBG << "[seccomp] seccomp_notify_receive failed with e=" << e;
				co_return;
			}

			resp->id	= req->id;
			resp->error = -EPERM;
			resp->val	= 0;

			switch (req->data.nr)
			{
				case SCMP_SYS(openat):
				{
					// req->data.args[1] 是待打开的文件名. 但是, 这个指针是在待打开的进程里的, 所以
					// 要使用跨进程 memcpy
					if (seccomp_notify_id_valid(seccomp_notify_fd.native_handle(), req->id) !=0)
						co_return;

					struct iovec this_readbuf = { openat_param1, sizeof (openat_param1) - 1 };
					struct iovec traced_readbuf = { reinterpret_cast<void*>(req->data.args[1]), 4096 };
					memset(openat_param1, 0 , sizeof openat_param1);
					process_vm_readv(req->pid, &this_readbuf, 1, &traced_readbuf, 1, 0);
					std::string node_want_open = openat_param1;

					int dirfd = static_cast<int>(req->data.args[0]);
					long open_flag = req->data.args[2];

					do
					{
						if ( (dirfd == AT_FDCWD && (open_flag == (O_RDONLY | O_CLOEXEC)))
							&& (
							node_want_open.starts_with("/usr")
							|| node_want_open.starts_with("/lib")
							|| node_want_open.starts_with("/etc/ld")
							|| node_want_open.starts_with("/run")
							|| node_want_open.starts_with("/etc/resolv.conf")
							|| node_want_open.starts_with("/etc/localtime")
							|| node_want_open.starts_with("/etc/ssl")
							|| node_want_open.starts_with("/etc/host.conf")
							|| node_want_open.starts_with("/etc/hosts")
							|| node_want_open.starts_with("/sys/fs/cgroup/memory/")
							|| node_want_open.starts_with("/proc/meminfo")
							|| node_want_open.starts_with("/etc/nsswitch.conf")
							|| node_want_open.starts_with("/etc/gai.conf")
							|| node_want_open.starts_with("/dev/")))
						{
							LOG_DBG << "[seccomp] node wants to open file " << openat_param1 << " allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}
						if ( ((dirfd == AT_FDCWD) && (open_flag == (O_RDONLY))) && (
								 node_want_open.starts_with("/etc/resolv.conf")
								|| node_want_open.starts_with("/etc/localtime")
								|| node_want_open.starts_with("/etc/ssl")
								|| node_want_open.starts_with("/etc/hosts")
								|| node_want_open.starts_with("/usr/share")
								|| node_want_open.starts_with("/etc/host.conf")
								|| node_want_open.starts_with("/etc/nsswitch.conf")
								|| node_want_open.starts_with("/etc/gai.conf")
								|| node_want_open.starts_with("/dev/urandom")))
						{
							LOG_DBG << "[seccomp] node wants to open file O_RDONLY " << openat_param1 << " allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}

						if (node_want_open.starts_with("/dev/") && (dirfd == AT_FDCWD))
						{
							LOG_DBG << "[seccomp] node wants to open dir /dev/ allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}

						if (node_want_open.starts_with("/dev/pts/") && (dirfd == AT_FDCWD))
						{
							LOG_DBG << "[seccomp] node wants to open dir /dev/pts/? allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}

						if (node_want_open.starts_with("/run/systemd/") && (dirfd == AT_FDCWD))
						{
							LOG_DBG << "[seccomp] node wants to open dir /run/systemd/ return ENOENT";
							resp->error = ENOENT;
							resp->flags = 0;
							break;
						}

						if (dirfd == AT_FDCWD && vfs.contains(node_want_open))
						{
							auto file_content = vfs.at(node_want_open);

							struct seccomp_notif_addfd addfd;
							addfd.id = req->id;
							addfd.srcfd = memfd_create("/thescript.js", 0);
							auto writed_bytes = write(addfd.srcfd, file_content.data(), file_content.size());
							lseek(addfd.srcfd, 0, SEEK_SET);
							addfd.newfd_flags = (open_flag & O_CLOEXEC )? O_CLOEXEC : 0;
							addfd.newfd = 0;
							addfd.flags = SECCOMP_ADDFD_FLAG_SEND;

							resp->val = ioctl(seccomp_notify_fd.native_handle(), SECCOMP_IOCTL_NOTIF_ADDFD, &addfd);
							resp->error = 0;
							resp->flags = 0;
							close(addfd.srcfd);

							LOG_ERR << "[seccomp] node wants to open file " << openat_param1 << " satified with VFS";
							break;;

						}

						if (resp->error != 0)
							LOG_ERR << "[seccomp] node wants to open file " << openat_param1 << " but open denied";
					}
					while(false);
				}break;
				case SCMP_SYS(connect):
				{
					socklen_t addrlen = static_cast<socklen_t>(req->data.args[2]);

					std::vector<char> raw_socket(addrlen);

					struct iovec this_readbuf = { raw_socket.data(), addrlen };
					struct iovec traced_readbuf = { reinterpret_cast<void*>(req->data.args[1]), addrlen };
					process_vm_readv(req->pid, &this_readbuf, 1, &traced_readbuf, 1, 0);

					if (raw_socket[0] == AF_UNSPEC)
					{
						resp->error = 0;
						resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
						break;

					}
					else if (raw_socket[0] == AF_LOCAL)
					{
						sockaddr_un * addr = reinterpret_cast<sockaddr_un *>(raw_socket.data());

						std::string_view unix_socket_path(addr->sun_path, 108);

						if (unix_socket_path.starts_with("/run/systemd/resolve"))
						{
							LOG_DBG << "[seccomp] node wants to connecto to systemd-resolved, allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}
						if (unix_socket_path.starts_with("/var/run/nscd/socket"))
						{
							LOG_DBG << "[seccomp] node wants to connecto to nscd, allowed";
							resp->error = 0;
							resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
							break;
						}
						// 拒绝 af_local
						break;
					}
					else if ((raw_socket[0] == AF_INET) && (addrlen == sizeof(sockaddr_in)))
					{
						sockaddr_in * addr = reinterpret_cast<sockaddr_in *>(raw_socket.data());
						auto destip = boost::asio::ip::make_address_v4(*
							reinterpret_cast<const boost::asio::ip::address_v4::bytes_type*>(&(addr->sin_addr))
						);

						LOG_DBG << "[seccomp] node wants to connect to " << destip.to_string() << ":" << ntohs(addr->sin_port);

						// 拒绝连接  5432
						if ( ntohs(addr->sin_port) == 5432)
							break;

						if (destip.is_multicast())
							break;

						// 终于允许连接了.
						resp->error = 0;
						resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
						break;
					}
					else if ((raw_socket[0] == AF_INET6) && (addrlen == sizeof(sockaddr_in6)))
					{
						sockaddr_in6 * addr = reinterpret_cast<sockaddr_in6 *>(raw_socket.data());
						auto destip = boost::asio::ip::make_address_v6(*
							reinterpret_cast<const boost::asio::ip::address_v6::bytes_type*>(&(addr->sin6_addr)),
							addr->sin6_scope_id
						);

						LOG_DBG << "[seccomp] node wants to connect to [" << destip.to_string() << "]:" << ntohs(addr->sin6_port);

						// 拒绝连接  5432
						if (addr->sin6_port == 5432)
							break;

						if (destip.is_multicast())
							break;
						if (destip.is_site_local())
							break;
						// 终于允许连接了.
						resp->error = 0;
						resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
						break;
					}
				}break;
				default:
					break;
			}

			if (resp->val!=0)
				continue;

			ret = seccomp_notify_respond(seccomp_notify_fd.native_handle(), resp);
			if (ret != 0)
				co_return;
		}
	}
}

#endif
