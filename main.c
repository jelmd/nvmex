/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License") 1.1!
 * You may not use this file except in compliance with the License.
 *
 * See  https://spdx.org/licenses/CDDL-1.1.html  for the specific
 * language governing permissions and limitations under the License.
 *
 * Copyright 2021 Jens Elkner (jel+nvmex-src@cs.ovgu.de)
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <prom.h>

#include <microhttpd.h>

#include "inspect.h"
#include "clocks.h"
#include "bar1memory.h"
#include "temperature.h"
#include "power.h"
#include "fan.h"
#include "util.h"
#include "pcie.h"
#include "violations.h"
#include "memory.h"
#include "ecc.h"
#include "nvlink.h"
#include "enc.h"
#include "fbc.h"

typedef enum {
	SMF_EXIT_OK	= 0,
	SMF_EXIT_ERR_OTHER,
	SMF_EXIT_ERR_FATAL = 95,
	SMF_EXIT_ERR_CONFIG,
	SMF_EXIT_MON_DEGRADE,
	SMF_EXIT_MON_OFFLINE,
	SMF_EXIT_ERR_NOSMF,
	SMF_EXIT_ERR_PERM,
	SMF_EXIT_TEMP_DISABLE,
	SMF_EXIT_TEMP_TRANSIENT
} SMF_EXIT_CODE;

static struct option options[] = {
	{"no-scrapetime",		no_argument,		NULL, 'L'},
	{"no-scrapetime-all",	no_argument,		NULL, 'S'},
	{"compact",				no_argument,		NULL, 'c'},
	{"daemon",				no_argument,		NULL, 'd'},
	{"foreground",			no_argument,		NULL, 'f'},
	{"help",				no_argument,		NULL, 'h'},
	{"logfile",				required_argument,	NULL, 'l'},
	{"no-metrics",			required_argument,	NULL, 'n'},
	{"port",				required_argument,	NULL, 'p'},
	{"source",				required_argument,	NULL, 's'},
	{"verbosity",			required_argument,	NULL, 'v'},
	{"version",				no_argument,		NULL, 'V'},
	{0, 0, 0, 0}
};

static const char *shortUsage = {
	"[-LScdfh] [-l file] [-n list] [-s ip] [-p port] [-v DEBUG|INFO|WARN|ERROR|FATAL]"
};

static struct {
	uint promflags;
	bool versionInfo;
	bool gpuInfo;
	bool clocks;
	bool bar1mem;
	bool temperature;
	bool power;
	bool fan;
	bool util;
	bool pcie;
	bool violations;
	bool memory;
	bool ecc;
	bool nvlink;
	bool encStats;
	bool encSessions;
	bool fbcStats;
	bool fbcSessions;
	gpu_t *devList;
	unsigned int devs;
	prom_counter_t *req_counter;
	prom_counter_t *res_counter;
	struct MHD_Daemon *daemon;
	uint port;
	struct in6_addr *addr;
	bool ipv6;
	int MHD_error;
	char *logfile;
} global = {
	.promflags = PROM_PROCESS | PROM_SCRAPETIME | PROM_SCRAPETIME_ALL,
	.versionInfo = true,
	.gpuInfo = true,
	.clocks = true,
	.bar1mem = true,
	.temperature = true,
	.power = true,
	.fan = true,
	.util = true,
	.pcie = true,
	.violations = true,
	.memory = true,
	.ecc = true,
	.nvlink = true,
	.encStats = true,
	.encSessions = true,
	.fbcStats = true,
	.fbcSessions = true,
	.devList = NULL,
	.devs = 0,
	.req_counter = NULL,
	.res_counter = NULL,
	.daemon = NULL,
	.port = 9400,
	.addr = NULL,
	.ipv6 = false,
	.MHD_error = -1,
	.logfile = NULL
};

static int
disableMetrics(char *clist) {
	char *s, *e;
	size_t len;
	int res = 0;

	if (clist == NULL)
		return 0;

	len = strlen(clist);
	if (len == 0)
		return 0;
	e = clist + len;
	s = e;
	while (s > clist) {
		s--;
		if (*s != ',' && s != clist)
			continue;
		if (s != clist)
			s++;
		if (s != e) {
			if (strcmp(s, "process") == 0)
				global.promflags &= ~PROM_PROCESS;
			else if (strcmp(s, "version") == 0)
				global.versionInfo = false;
			else if (strcmp(s, "gpuinfo") == 0)
				global.gpuInfo = false;
			else if (strcmp(s, "clock") == 0)
				global.clocks = false;
			else if (strcmp(s, "bar1mem") == 0)
				global.bar1mem = false;
			else if (strcmp(s, "temperature") == 0)
				global.temperature = false;
			else if (strcmp(s, "power") == 0)
				global.power = false;
			else if (strcmp(s, "fan") == 0)
				global.fan = false;
			else if (strcmp(s, "utilization") == 0)
				global.util = false;
			else if (strcmp(s, "pcie") == 0)
				global.pcie = false;
			else if (strcmp(s, "violation") == 0)
				global.violations = false;
			else if (strcmp(s, "memory") == 0)
				global.memory = false;
			else if (strcmp(s, "ecc") == 0)
				global.ecc = false;
			else if (strcmp(s, "nvlink") == 0)
				global.nvlink = false;
			else if (strcmp(s, "encstat") == 0)
				global.encStats = false;
			else if (strcmp(s, "encsession") == 0)
				global.encSessions = false;
			else if (strcmp(s, "fbcstat") == 0)
				global.fbcStats = false;
			else if (strcmp(s, "fbcsession") == 0)
				global.fbcSessions = false;
			else {
				PROM_WARN("Unknown metrics '%s'", s);
				res++;
			}
		}
		if (s != clist)
			s--;
		*s = '\0';
		e = s;
	}
	return res;
}

// Just in case, someone switches to MHD_USE_THREAD_PER_CONNECTION
static _Thread_local psb_t *sb = NULL;

static prom_map_t *
collect(prom_collector_t *self) {
	bool compact = global.promflags & PROM_COMPACT;
	PROM_DEBUG("collector: %p  sb: %p", self, sb);
	if (global.versionInfo)
		getVersions(sb, compact);
	if (global.gpuInfo)
		getDevInfos(sb, compact, global.devs, global.devList);
	if (global.clocks)
		getClocks(sb, compact, global.devs, global.devList);
	if (global.bar1mem)
		getBar1memory(sb, compact, global.devs, global.devList);
	if (global.temperature)
		getTemperatures(sb, compact, global.devs, global.devList);
	if (global.power)
		getPower(sb, compact, global.devs, global.devList);
	if (global.fan)
		getFan(sb, compact, global.devs, global.devList);
	if (global.util)
		getUtilization(sb, compact, global.devs, global.devList);
	if (global.pcie)
		getPCIe(sb, compact, global.devs, global.devList);
	if (global.violations)
		getViolations(sb, compact, global.devs, global.devList);
	if (global.memory)
		getMemory(sb, compact, global.devs, global.devList);
	if (global.ecc)
		getECC(sb, compact, global.devs, global.devList);
	if (global.nvlink)
		getNvLink(sb, compact, global.devs, global.devList);
	if (global.encStats || global.encSessions)
		getEnc(sb, compact, global.devs, global.devList, global.encSessions);
	if (global.fbcStats || global.fbcSessions)
		getFBC(sb, compact, global.devs, global.devList, global.fbcSessions);
	if (sb != NULL && !compact)
		psb_add_char(sb, '\n');
	return NULL;
}

// generate the short option string for getopts from <opts>
static char *
getShortOpts(const struct option *opts) {
	int i, k = 0, len = 0;
	char *str;

	while (opts[len].name != NULL)
		len++;
	str = malloc(sizeof(char) * len  * 2 + 1);
	if (str == NULL)
		return NULL;

	str[k++] = '+';		// POSIXLY_CORRECT
	for (i = 0; i < len; i++) {
		str[k++] = opts[i].val;
		if (opts[i].has_arg == required_argument)
			str[k++] = ':';
	}
	str[k] = '\0';
	return str;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static int
http_handler(void *cls, struct MHD_Connection *connection, const char *url,
	const char *method, const char *version, const char *upload_data,
	size_t *upload_data_size, void **con_cls)
{
#pragma GCC diagnostic pop
	char *body, *s;
	size_t len;
	struct MHD_Response *response;
	enum MHD_ResponseMemoryMode mode = MHD_RESPMEM_PERSISTENT;
	unsigned int status = MHD_HTTP_BAD_REQUEST;
	static const char *labels[] = { "" };
	static char *RESP[] = { NULL, NULL, NULL };
	static int rlen[] = { 0, 0, 0 };

	int ret;

	if (RESP[0] == NULL) {
		RESP[0]= strdup("Invalid HTTP Method\n");
		rlen[0] = strlen(RESP[0]);
		RESP[1]= strdup("<html><body>See <a href='/metrics'>/metrics</a>.\r\n");
		rlen[1] = strlen(RESP[1]);
		RESP[2]= strdup("Bad Request\n");
		rlen[2] = strlen(RESP[2]);
	}

	if (strcmp(method, "GET") != 0) {
		body = RESP[0];
		len = rlen[0];
		labels[0] = "other";
	} else if (strcmp(url, "/") == 0) {
		body = RESP[1];
		len = rlen[1];
		status = MHD_HTTP_OK;
		labels[0] = "/";
	} else if (strcmp(url, "/metrics") == 0) {
		// trick 17: collect() adds stuff to sb directly, when it gets invoked
		// indirectly by pcr_bridge(). Therefore: thread local
		if (sb != NULL)
			PROM_WARN("stringBuilder %p is already there =8-(", sb);
		sb = psb_new();
		s = pcr_bridge(PROM_COLLECTOR_REGISTRY);
		psb_add_str(sb, s);		// add libprom metrics
		free(s);				// avoid mem leaks
		body = psb_dump(sb);
		len = psb_len(sb);
		psb_destroy(sb);		// avoid mem leaks on thread exit
		sb = NULL;
		labels[0] = "/metrics";
		mode = MHD_RESPMEM_MUST_FREE;
		status = MHD_HTTP_OK;
	} else {
		body = RESP[2];
		len = rlen[2];
		labels[0] = "other";
	}
	prom_counter_inc(global.req_counter, labels);

	response = MHD_create_response_from_buffer(len, body, mode);
	if (response == NULL) {
		if (mode == MHD_RESPMEM_MUST_FREE)
			free(body);
		ret = MHD_NO;
	} else {
		labels[0] = "count";
		prom_counter_inc(global.res_counter, labels);
		labels[0] = "bytes";
		prom_counter_add(global.res_counter, len, labels);
		ret = MHD_queue_response(connection, status, response);
		MHD_destroy_response(response);
	}
	return ret;
}

// redirect MHD_DLOG to prom_log
static void
MHD_logger(void *cls, const char *fmt, va_list ap) {
	static char s[256];

	// the experimental API has loglevel decision support, but it is usually n/a
	(void) cls;		// unused
	vsnprintf(s, sizeof(s), fmt, ap);
	// since MHD does not return error details but usually logs the reason for
	// an error before polluting errno again, we capture it here. At least for
	// MHD_start_daemon() it should be sufficient.
	global.MHD_error = errno;
	prom_log(PLL_WARN, (const char*) s);
}

static int
setupProm(void) {
	static const char *keys[] = { NULL };
	prom_collector_t* pc = NULL;
	prom_counter_t *reqc, *resc;
	reqc = resc = NULL;

	if (pcr_init(global.promflags, "nvmex_"))
		return 1;

	keys[0] = "url";
	if((global.req_counter = prom_counter_new("request_total",
		"Number of HTTP requests seen since the start of the exporter "
		"excl. the current one.",
		1, keys)) == NULL)
		goto fail;
	reqc = global.req_counter;
	if (pcr_register_metric(global.req_counter))
		goto fail;
	reqc = NULL;

	keys[0] = "type";
	if ((global.res_counter = prom_counter_new("response_total",
		"HTTP responses by count and bytes excl. this response and "
		"HTTP headers seen since the start of the exporter.",
		1, keys)) == NULL)
		goto fail;
	resc = global.res_counter;
	if (pcr_register_metric(global.res_counter))
		goto fail;
	resc = NULL;

	pc = prom_collector_new("nvidia");
	if (pc == NULL)
		goto fail;
	prom_collector_set_collect_fn(pc, &collect);
	if (pcr_register_collector(PROM_COLLECTOR_REGISTRY, pc) == 0)
		pc = NULL;
	else
		goto fail;
	return 0;

fail:
	if (pc != NULL)
		prom_collector_destroy(pc);
	if (reqc != NULL)
		prom_counter_destroy(reqc);
	if (resc != NULL)
		prom_counter_destroy(resc);
	global.req_counter = global.res_counter = NULL;
	pcr_destroy(PROM_COLLECTOR_REGISTRY);
	return 1;
}

static void
cleanupProm(void) {
	pcr_destroy(PROM_COLLECTOR_REGISTRY);
	global.req_counter = global.res_counter = NULL;
}

static int
startHttpServer(void) {
	struct sockaddr *addr = NULL;
	uint flags = MHD_USE_DEBUG;	// same as MHD_USE_ERROR_LOG but backward comp.
	// since there is no way to use a blocking, i.e. one (this) thread only
	// MHD_run(), or MHD_{e?poll|select}, or MHD_polling_thread.
	// same as MHD_USE_INTERNAL_POLLING_THREAD but backward compatible
	flags |= MHD_USE_SELECT_INTERNALLY;
	if (MHD_is_feature_supported(MHD_FEATURE_EPOLL) == MHD_YES)
		flags |= MHD_USE_EPOLL;
	else if (MHD_is_feature_supported(MHD_FEATURE_POLL) == MHD_YES)
		flags |= MHD_USE_POLL;

	if (global.addr != NULL) {
		struct sockaddr_in v4addr;
		struct sockaddr_in6 v6addr;
		size_t len;
		char buf[64];

		buf[0] = '\0';
		if (global.ipv6) {
			flags |= MHD_USE_IPv6;
			len = sizeof (struct sockaddr_in6);
			inet_ntop(AF_INET6, global.addr, buf, len);
			memset(&v6addr, 0, len);
			v6addr.sin6_family = AF_INET6;
			v6addr.sin6_port = htons (global.port);
			memcpy(&(v6addr.sin6_addr), global.addr, sizeof(struct in6_addr)); 
			addr = (struct sockaddr *) &v6addr;
		} else {
			len = sizeof (struct sockaddr_in);
			inet_ntop(AF_INET, global.addr, buf, len);
			memset(&v4addr, 0, len);
			v4addr.sin_family = AF_INET;
			v4addr.sin_port = htons (global.port);
			memcpy(&(v4addr.sin_addr), global.addr, sizeof(struct in_addr)); 
			addr = (struct sockaddr *) &v4addr;
		}
		PROM_INFO("Listening on IP%s: %s:%u", global.ipv6 ? "v6" : "v4", buf,
			global.port);
	} else {
		PROM_INFO("Listening on IPv4: 0.0.0.0:%u", global.port);
	}

	global.daemon = MHD_start_daemon(flags, global.port,
		/* checkClientFN */ NULL, /* checkClientFN arg */ NULL,
		/* requestHandler */ &http_handler, /* requestHandler arg */ NULL,
		MHD_OPTION_EXTERNAL_LOGGER, &MHD_logger, /* logstream */ NULL,
		MHD_OPTION_SOCK_ADDR, addr,
		MHD_OPTION_END);
	if (global.daemon == NULL) {
		PROM_FATAL("Unable to start http daemon.", "");
		return global.MHD_error == EACCES
			? SMF_EXIT_ERR_PERM
			: SMF_EXIT_ERR_OTHER;
	}
	return SMF_EXIT_OK;
}

static int
daemonize(void) {
	int status;
	int pfd[2];
	pid_t pid;
	sigset_t sset;
	sigset_t oset;

	// During init phase block all sigs except ABRT. They get unblocked on the
	// child once it has notified the parent about its status, and the parent
	// exits.
	(void) sigfillset(&sset);
	(void) sigdelset(&sset, SIGABRT);
	(void) sigprocmask(SIG_BLOCK, &sset, &oset);

	// comm channel between parent and child
	if (pipe(pfd) == -1) {
		PROM_FATAL("Unable to create pipe (%s)", strerror(errno));
		exit(SMF_EXIT_ERR_OTHER);
	}

	if ((pid = fork()) == -1) {
		PROM_FATAL("Unable to fork process (%s)", strerror(errno));
		exit(SMF_EXIT_ERR_OTHER);
	}

	// parent: wait for the status message from the child and exit immediately
    if (pid > 0) {
		(void) close(pfd[1]);
		if (read(pfd[0], &status, sizeof (status)) == sizeof (status))
			_exit(status);
		// just in case, comm failed
		if ((waitpid(pid, &status, 0) == pid) && WIFEXITED(status))
			_exit(WEXITSTATUS(status));
		PROM_FATAL("Failed to spawn daemon process.", "");
		_exit(SMF_EXIT_ERR_OTHER);
	}

	// child: cleanup and detach
	(void) setsid();
	(void) chdir("/");
	(void) umask(022);
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) close(pfd[0]);
	(void) close(0);
	(void) close(1);
	(void) close(2);
	// just in case NVML or microhttpd use it somewhere
	(void) open("/dev/null", O_RDONLY);
	if (global.logfile == NULL) {
		(void) open("/dev/null", O_WRONLY);
		(void) open("/dev/null", O_WRONLY);
	} else {
		(void) open(global.logfile, O_WRONLY | O_APPEND);
		(void) open(global.logfile, O_WRONLY | O_APPEND);
	}

	return pfd[1];
}

int
main(int argc, char **argv) {
	uint n, mode = 0;	// 0 .. oneshot  1 .. foreground  2 .. daemon
	int err = 0, res, pfd = -1, status = 0;
	struct in_addr inaddr;
	struct in6_addr in6addr;
	struct in6_addr *addr = malloc(sizeof(struct in6_addr));
	psb_t *buf;
	char *str = getShortOpts(options);

	while (1) {
		int c, optidx = 0;

		if (str == NULL)
			break;
		c = getopt_long (argc, argv, str, options, &optidx);
		if (c == -1)
			break;
		switch (c) {
			case 'V':
				fputs("nvmex " NVMEX_VERSION
					"\n(C) 2021 " NVMEX_AUTHOR "\n", stdout);
				return 0;
			case 'L':
				global.promflags &= ~PROM_SCRAPETIME;
				break;
			case 'S':
				global.promflags &= ~PROM_SCRAPETIME_ALL;
				break;
			case 'c':
				global.promflags |= PROM_COMPACT;
				break;
			case 'd':
				mode = 2;
				break;
			case 'f':
				mode = 1;
				break;
			case 'h':
				fprintf(stderr, "Usage: %s %s\n", argv[0], shortUsage);
				return 0;
			case 'l':
				if (global.logfile != NULL)
					free(global.logfile);
				global.logfile = strdup(optarg);
				break;
			case 'n':
				err += disableMetrics(optarg);
				break;
			case 'p':
				if ((sscanf(optarg, "%u", &n) != 1) || n == 0) {
					fprintf(stderr, "Invalid port '%s'.\n", optarg);
					err++;
				} else {
					global.port = n;
				}
				break;
			case 's':
				if (strstr(optarg, ":") == NULL) {
					if ((res = inet_pton(AF_INET, optarg, &inaddr)) == 1)
						memcpy(addr, &inaddr, sizeof(struct in_addr));
				} else if ((res = inet_pton(AF_INET6, optarg, &in6addr)) == 1) {
					if (MHD_is_feature_supported(MHD_FEATURE_IPv6) == MHD_NO) {
						fprintf(stderr, "libmicrohttpd has no IPv6 support");
						res = 0;
					} else {
						memcpy(addr, &in6addr, sizeof(struct in6_addr));
						global.ipv6 = true;
					}
				}
				if (res != 1) {
					fprintf(stderr, "Invalid IP address '%s'.", optarg);
					err++;
				} else {
					global.addr = addr;
					addr = NULL;
				}
				break;
			case 'v':
				n = prom_log_level_parse(optarg);
				if (n == 0) {
					fprintf(stderr,"Invalid log level '%s'.\n",optarg);
					err++;
				} else {
					prom_log_level(n);
				}
				break;
			case '?':
				fprintf(stderr, "Usage: %s %s\n", argv[0], shortUsage);
				return(1);
		}
	}
	free(str);
	free(addr);
	if (err)
		return SMF_EXIT_ERR_CONFIG;

	if (global.logfile != NULL) {
		FILE *logfile = fopen(global.logfile, "a");
		if (logfile != NULL)
			prom_log_use(logfile);
		else {
			fprintf(stderr, "Unable to open logfile '%s': %s\n",
				global.logfile, strerror(errno));
			return (errno == EACCES) ? SMF_EXIT_ERR_PERM : SMF_EXIT_ERR_CONFIG;
		}
	}

	if (mode == 2)
		pfd = daemonize();

	if (start()) {
		status = SMF_EXIT_TEMP_DISABLE;
		if (mode == 2) {
			(void) write(pfd, &status, sizeof (status));
			(void) close(pfd);
		}
		return status;
	}
	// init
	buf = psb_new(); // prevent that prom formatted output goes to stdout
	str = getVersions(buf, global.promflags & PROM_COMPACT);
	fprintf(stderr, "%s", str);
	getUnitInfos(NULL);
	global.devs = getDevices(&global.devList);
	if (global.devs > 0) {
		str = getDevInfos(buf, global.promflags & PROM_COMPACT,
			global.devs, global.devList);
		fprintf(stderr, "\nDevices:\n%s", str);
		if (mode == 0) {
			collect(NULL);
			status = SMF_EXIT_OK;
		} else if (setupProm() == 0) {
			fputs("\n", stderr);
			status = startHttpServer();
			// let the parent exit
			if (mode == 2) {
				(void) write(pfd, &status, sizeof (status));
				(void) close(pfd);
			}
			// because libmicrohttpd does not expose blocking calls =8-((((
			if (status == SMF_EXIT_OK)
				pause();
		} else {
			status = SMF_EXIT_ERR_OTHER;
			if (mode == 2) {
				(void) write(pfd, &status, sizeof (status));
				(void) close(pfd);
			}
		}
	} else {
		fputs("Nothing todo - exiting.\n", stderr);
		status = SMF_EXIT_TEMP_DISABLE;
		if (mode == 2) {
			(void) write(pfd, &status, sizeof (status));
			(void) close(pfd);
		}
	}
	// finally
	psb_destroy(buf);
	cleanupProm();
	free(global.addr);
	global.devs = cleanup(global.devs, &global.devList);
	stop();
	return status;
}
