#include "sysctl_kern.h"
#include "getrlimit.h"
#include "../ext/sysinfo.h"
#include "../ext/syslog.h"
#include "../ext/sys/utsname.h"
#include <linux-syscalls/linux.h>
#include "../time/gettimeofday.h"
#include "darling-config.h"
#include "../errno.h"
#include <sys/errno.h>

enum {
	_KERN_MSGBUF = 1000,
};

static sysctl_handler(handle_msgbuf);
static sysctl_handler(handle_proc);
static sysctl_handler(handle_procargs32);
static sysctl_handler(handle_argmax);
static sysctl_handler(handle_boottime);
static sysctl_handler(handle_ostype);
static sysctl_handler(handle_hostname);
static sysctl_handler(handle_domainname);
static sysctl_handler(handle_osrelease);
static sysctl_handler(handle_version);

extern int _sysctl_proc(int what, int flag, struct kinfo_proc* out, unsigned long* buflen);
extern int _sysctl_procargs(int pid, char* buf, unsigned long* buflen);
extern int strlcpy(char* dst, const char* src, __SIZE_TYPE__ dst_size);
extern __SIZE_TYPE__ strlen(const char* str);

const struct known_sysctl sysctls_kern[] = {
	{ .oid = _KERN_MSGBUF, .type = CTLTYPE_STRING, .exttype = "S", .name = "msgbuf", .handler = handle_msgbuf },
	{ .oid = KERN_PROC, .type = CTLTYPE_STRUCT, .exttype = "", .name = "proc", .handler = handle_proc },
	{ .oid = KERN_PROCARGS2, .type = CTLTYPE_STRUCT, .exttype = "", .name = "procargs2", .handler = handle_procargs32 },
	{ .oid = KERN_ARGMAX, .type = CTLTYPE_INT, .exttype = "I", .name = "argmax", .handler = handle_argmax },
	{ .oid = KERN_BOOTTIME, .type = CTLTYPE_STRUCT, .exttype = "S,timeval", .name = "boottime", .handler = handle_boottime },
	{ .oid = KERN_OSTYPE, .type = CTLTYPE_STRING, .exttype = "S", .name = "ostype", .handler = handle_ostype },
	{ .oid = KERN_HOSTNAME, .type = CTLTYPE_STRING, .exttype = "S", .name = "hostname", .handler = handle_hostname },
	{ .oid = KERN_DOMAINNAME, .type = CTLTYPE_STRING, .exttype = "S", .name = "domainname", .handler = handle_domainname },
	{ .oid = KERN_OSRELEASE, .type = CTLTYPE_STRING, .exttype = "S", .name = "osrelease", .handler = handle_osrelease },
	{ .oid = KERN_VERSION, .type = CTLTYPE_STRING, .exttype = "S", .name = "version", .handler = handle_version },
	{ .oid = -1 }
};

sysctl_handler(handle_msgbuf)
{
	if (*oldlen <= 8)
	{
		// Caller is asking for buffer size
		if (*oldlen < sizeof(int))
			return -EINVAL;

		*((int*) old) = __linux_syslog(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
	}
	else
	{
		// Caller is asking for buffer contents
		int ret;

		ret = __linux_syslog(SYSLOG_ACTION_READ_ALL, (char*) old, *oldlen);
		if (ret < 0)
			return errno_linux_to_bsd(ret);

		*oldlen = ret;
	}
	return 0;
}

sysctl_handler(handle_proc)
{
	if (nlen < 4)
		return -ENOENT;
	return _sysctl_proc(name[2], name[3], (struct kinfo_proc*) old, oldlen);
}

sysctl_handler(handle_procargs32)
{
	if (nlen < 3)
		return -ENOENT;
	return _sysctl_procargs(name[2], (char*) old, oldlen);
}

sysctl_handler(handle_argmax)
{
	struct rlimit lim;
	int r;
	int* ovalue = (int*) old;

	r = sys_getrlimit(BSD_RLIMIT_STACK, &lim);
	if (r < 0)
		return r;

	if (ovalue != NULL)
		*ovalue = lim.rlim_cur / 4;
	*oldlen = sizeof(int);
	return 0;
}

sysctl_handler(handle_boottime)
{
	// Fill in struct timeval { tv_sec, tv_usec }
	struct sysinfo info;
	struct bsd_timeval* tv;

	if (__linux_sysinfo(&info) == -1)
		return -errno;

	tv = (struct bsd_timeval*) old;
	if (tv != NULL)
	{
		if (*oldlen < sizeof(*tv))
			return -EINVAL;
		sys_gettimeofday(tv, NULL);

		tv->tv_sec -= info.uptime;
	}
	*oldlen = sizeof(*tv);
	
	return 0;
}

sysctl_handler(handle_ostype)
{
	const char* s = EMULATED_SYSNAME;

	copyout_string(s, (char*) old, oldlen);
	return 0;
}

sysctl_handler(handle_osrelease)
{
	const char* s = EMULATED_RELEASE;

	copyout_string(s, (char*) old, oldlen);
	return 0;
}

sysctl_handler(handle_version)
{
	const char* s = EMULATED_VERSION;

	copyout_string(s, (char*) old, oldlen);
	return 0;
}

static struct linux_utsname lu;
struct linux_utsname* need_uname(void)
{
#ifndef VARIANT_DYLD
	// Cache __linux_uname results
	if (!lu.sysname[0])
	{
		__linux_uname(&lu);
	}
#endif
	return &lu;
}

sysctl_handler(handle_hostname)
{
	need_uname();
	copyout_string(lu.nodename, (char*) old, oldlen);
	return 0;
}

sysctl_handler(handle_domainname)
{
	need_uname();
	copyout_string(lu.domainname, (char*) old, oldlen);
	return 0;
}
