#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <fcntl.h>

#include "compiler.h"
#include "types.h"
#include "util.h"

#define DEFAULT_LOGLEVEL	LOG_WARN
#define DEFAULT_LOGFD		STDERR_FILENO

static unsigned int current_loglevel = DEFAULT_LOGLEVEL;
static int current_logfd = DEFAULT_LOGFD;

int log_get_fd(void)
{
	return current_logfd;
}

int log_init(const char *output)
{
	struct rlimit rlimit;
	int new_logfd = DEFAULT_LOGFD;

	if (getrlimit(RLIMIT_NOFILE, &rlimit)) {
		pr_perror("Can't get rlimit");
		return -1;
	}

	/*
	 * We might need to transfer this descriptors
	 * to another process' address space (and file
	 * descriptors space) so we try to minimize
	 * potential conflict between descriptors and
	 * try to reopen them somewhere near a limit.
	 *
	 * Still an explicit output file might be
	 * requested.
	 */

	if (output) {
		new_logfd = open(output, O_CREAT | O_WRONLY);
		if (new_logfd < 0) {
			pr_perror("Can't create log file %s", output);
			return -1;
		}
	}

	current_logfd = rlimit.rlim_cur - 1;
	if (reopen_fd_as(current_logfd, new_logfd) < 0)
		goto err;

	return 0;

err:
	pr_perror("Log engine failure, can't duplicate descriptor");
	return -1;
}

void log_fini(void)
{
	if (current_logfd > 2)
		close_safe(&current_logfd);

	current_logfd = DEFAULT_LOGFD;
}

void log_set_loglevel(unsigned int level)
{
	if (!level)
		current_loglevel = DEFAULT_LOGLEVEL;
	else
		current_loglevel = level;
}

void print_on_level(unsigned int loglevel, const char *format, ...)
{
	va_list params;
	int fd;

	if (unlikely(loglevel == LOG_MSG)) {
		fd = STDOUT_FILENO;
	} else {
		if (loglevel > current_loglevel)
			return;
		fd = current_logfd;
	}

	va_start(params, format);
	vdprintf(fd, format, params);
	va_end(params);
}
