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

#include <stdlib.h>
#include <string.h>
#include <values.h>
#include <assert.h>

#include "clocks.h"

#ifndef MININT
#define MININT (-MAXINT - 1)
#endif

static const char *domain[] = { "GRAPHICS", "SM", "MEM", "VIDEO" };

static void
getMinMaxClock(nvmlDevice_t gpu, int *minMem, int *maxMem,
	int *minGra, int *maxGra)
{
	nvmlReturn_t res;
	uint *clocks, k;

	*maxMem = *maxGra = MININT;
	*minMem = *minGra = MAXINT;
	// list of allowed memory clocks. Kepler+
	clocks = NULL;
	k = 0;
	res = nvmlDeviceGetSupportedMemoryClocks(gpu, &k, clocks);
	if (NVML_ERROR_INSUFFICIENT_SIZE == res) {
		clocks = malloc(sizeof(uint) * k);
		res = nvmlDeviceGetSupportedMemoryClocks(gpu, &k, clocks);
	}
	if (NVML_SUCCESS != res || k == 0) {
		PROM_DEBUG("%s.clock min/max: %s",domain[NVML_CLOCK_MEM],nverror(res));
		goto end;
	}

	*maxMem = clocks[0];
	*minMem = clocks[k-1];
	PROM_DEBUG("Mem: Min=%d max=%d", *minMem, *maxMem);
	free(clocks);

	clocks = NULL;
	k = 0;
	res = nvmlDeviceGetSupportedGraphicsClocks(gpu, *minMem, &k, clocks);
	if (NVML_ERROR_INSUFFICIENT_SIZE == res) {
		clocks = malloc(sizeof(uint) * k);
		res = nvmlDeviceGetSupportedGraphicsClocks(gpu, *minMem, &k, clocks);
	}
	if (NVML_SUCCESS == res && k > 0)
		*minGra = clocks[k-1];
	else
		PROM_DEBUG("%s.clock min: %s",domain[NVML_CLOCK_GRAPHICS],nverror(res));
	free(clocks);

	clocks = NULL;
	k = 0;
	res = nvmlDeviceGetSupportedGraphicsClocks(gpu, *maxMem, &k, clocks);
	if (NVML_ERROR_INSUFFICIENT_SIZE == res) {
		clocks = malloc(sizeof(uint) * k);
		res = nvmlDeviceGetSupportedGraphicsClocks(gpu, *maxMem, &k, clocks);
	}
	if (NVML_SUCCESS == res && k > 0)
		*maxGra = clocks[0];
	else
		PROM_DEBUG("%s.clock max: %s",domain[NVML_CLOCK_GRAPHICS],nverror(res));
	PROM_DEBUG("Graphics: Min=%d max=%d", *minGra, *maxGra);

end:
	free(clocks);
	clocks = NULL;
}

static uint
getBoostClock(nvmlDevice_t gpu, uint clock) {
	nvmlReturn_t res;
	uint clockMHz;

	// maximum clock speeds for the device. Fermi+
	res = nvmlDeviceGetMaxClockInfo(gpu, clock, &clockMHz);
	if (NVML_SUCCESS == res && clockMHz != 0) {
		PROM_DEBUG("maxClock[%u] = %u", clock, clockMHz);
		return clockMHz;
	}

	// customer defined max boost clock. Pascal+
	res = nvmlDeviceGetMaxCustomerBoostClock(gpu, clock, &clockMHz);
	if (NVML_SUCCESS == res) {
		PROM_DEBUG("maxBoost[%u] = %u", clock, clockMHz);
		return clockMHz;
	}
	return 0;
}

static void
setStaticClockVals(gpu_t *gpu, bool compact) {
	nvmlReturn_t res;
	char buf[MBUF_SZ << 1], *p;
	int minMem, maxMem, minGra, maxGra;
	uint boost, k;

	if (gpu->minMaxClock == NULL) {
		size_t sz = sizeof(char *) * NVML_CLOCK_COUNT;
		char **c = (char **) malloc(sz);
		if (c == NULL)
			return;
		memset(c, 0, sz);
		gpu->minMaxClock = c;
		c = (char **) malloc(sz);
		if (c == NULL)
			return;
		memset(c, 0, sz);
		gpu->defaultClock = c;
	}
	PROM_DEBUG("Getting static clocks for gpu[%u]", gpu->idx);

	getMinMaxClock(gpu->dev, &minMem, &maxMem, &minGra, &maxGra);

	k = NVML_CLOCK_MEM;
	boost = getBoostClock(gpu->dev, k);
	if (maxMem == MININT)
		maxMem = boost;
	else if (boost == 0)
		boost = maxMem;
	if (maxMem != (int) boost) {
		PROM_WARN("mem.clock.max != mem.clock.boost (%d != %d)", maxMem, boost);
		if ((int) boost > maxMem)
			maxMem = boost;
	}
	p = buf;
	if (maxMem > 0) {
		p += sprintf(buf, NVMEXM_CLOCK_N
			"{gpu=\"%d\",domain=\"%s\",clock=\"max\",uuid=\"%s\"} %d\n",
			gpu->idx, domain[k], gpu->uuid, maxMem);
	} else if (compact) {
		buf[0] = '\0';
	} else {
		p += sprintf(buf, "# %s.clock.max n/a\n", domain[k]);
	}
	if (minMem != MAXINT) {
		p += snprintf(p, sizeof(buf) - (p - buf), NVMEXM_CLOCK_N
			"{gpu=\"%d\",domain=\"%s\",clock=\"min\",uuid=\"%s\"} %d\n",
			gpu->idx, domain[k], gpu->uuid, minMem);
	} else if (!compact) {
		p += snprintf(p, sizeof(buf) - (p - buf), "# %s.clock.min n/a\n",
			domain[k]);
	}
	gpu->minMaxClock[k] = strdup(buf);

	k = NVML_CLOCK_GRAPHICS;
	boost = getBoostClock(gpu->dev, k);
	if (maxGra == MININT)
		maxGra = boost;
	else if (boost == 0)
		boost = maxGra;
	if (maxGra != (int) boost) {
		PROM_WARN("gra.clock.max != gra.clock.boost (%d != %d)", maxGra, boost);
		if ((int) boost > maxGra)
			maxGra = boost;
	}
	p = buf;
	if (maxGra > 0) {
		p += sprintf(p, NVMEXM_CLOCK_N
			"{gpu=\"%d\",domain=\"%s\",clock=\"max\",uuid=\"%s\"} %d\n",
			gpu->idx, domain[k], gpu->uuid, maxGra);
	} else if (compact) {
		buf[0] = '\0';
	} else {
		p += sprintf(p, "# %s.clock.max n/a\n", domain[k]);
	}
	if (minGra != MAXINT) {
		p += snprintf(p, sizeof(buf) - (p - buf), NVMEXM_CLOCK_N
			"{gpu=\"%d\",domain=\"%s\",clock=\"min\",uuid=\"%s\"} %d\n",
			gpu->idx, domain[k], gpu->uuid, minGra);
	} else if (!compact) {
		p += snprintf(p, sizeof(buf)-(p-buf),"# %s.clock.min n/a\n", domain[k]);
	}
	gpu->minMaxClock[k] = strdup(buf);

	for (k = 0; k < NVML_CLOCK_COUNT; k++) {
		// value that GPU boots with or defaults to. Kepler+
		res = nvmlDeviceGetDefaultApplicationsClock(gpu->dev, k, &boost);
		if (NVML_SUCCESS == res) {
			sprintf(buf, NVMEXM_CLOCK_N
				"{gpu=\"%d\",domain=\"%s\",clock=\"default\",uuid=\"%s\"} %u\n",
				gpu->idx, domain[k], gpu->uuid, boost);
		} else if (compact) {
			buf[0] = '\0';
		} else {
			sprintf(buf, "# %s.clock.default n/a\n", domain[k]);
		}
		gpu->defaultClock[k] = strdup(buf);

		if (k == NVML_CLOCK_MEM || k == NVML_CLOCK_GRAPHICS)
			continue;
		boost = getBoostClock(gpu->dev, NVML_CLOCK_GRAPHICS);
		if (boost == 0) {
			if (compact)
				buf[0] = '\0';
			else
				sprintf(buf, "# %s.clock.max n/a\n", domain[k]);
		} else {
			sprintf(buf, NVMEXM_CLOCK_N
				"{gpu=\"%d\",domain=\"%s\",clock=\"max\",uuid=\"%s\"} %u\n",
				gpu->idx, domain[k], gpu->uuid, boost);
		}
		gpu->minMaxClock[k] = strdup(buf);
	}
}

bool
getClocks(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	uint clockMHz, i, k;
	char buf[MBUF_SZ];
	gpu_t *gpu;
	bool free_sb = sb == NULL;
	size_t sz;

	assert(NVML_CLOCK_COUNT == 4);

	if (devs == 0)
		return false;

	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_CLOCK);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL)
			continue;
		if (gpu->minMaxClock == NULL || gpu->defaultClock == NULL)
			setStaticClockVals(gpu, compact);
		for (k = 0; k < NVML_CLOCK_COUNT; k++) {
			psb_add_str(sb, (gpu->defaultClock)[k]);
			psb_add_str(sb, gpu->minMaxClock[k]);
			// current clock speed for the device. Fermi+
			res = nvmlDeviceGetClockInfo(gpu->dev, k, &clockMHz);
			if (NVML_SUCCESS == res) {
				snprintf(buf, MBUF_SZ, NVMEXM_CLOCK_N
				"{gpu=\"%d\",domain=\"%s\",clock=\"now\",uuid=\"%s\"} %u\n",
				i, domain[k], gpu->uuid, clockMHz);
				psb_add_str(sb, buf);
			}
			// value to use unless an overspec situation. Kepler+
			res = nvmlDeviceGetApplicationsClock(gpu->dev, k, &clockMHz);
			if (NVML_SUCCESS == res) {
				snprintf(buf, MBUF_SZ, NVMEXM_CLOCK_N
				"{gpu=\"%d\",domain=\"%s\",clock=\"set\",uuid=\"%s\"} %u\n",
				i, domain[k], gpu->uuid, clockMHz);
				psb_add_str(sb, buf);
			}
		}
	}
	if (!compact)
		addPromInfo(NVMEXM_CLOCK_THROTTLE);
	for (i = 0; i < devs; i++) {
		unsigned long long reasons;
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasClockThrottle == -1)
			continue;
		res = nvmlDeviceGetCurrentClocksThrottleReasons(gpu->dev, &reasons);
		if (NVML_SUCCESS == res) {
			snprintf(buf, MBUF_SZ,
				NVMEXM_CLOCK_THROTTLE_N "{gpu=\"%d\",uuid=\"%s\"} %lld\n",
				i, gpu->uuid, reasons);
			psb_add_str(sb, buf);
			gpu->hasClockThrottle = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasClockThrottle = -1", "");
			gpu->hasClockThrottle = -1;
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
