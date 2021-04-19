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

#include <assert.h>

#include "violations.h"

static const char *pname[] = {
	"POWER", "THERMAL", "SYNC_BOOST", "BOARD_LIMIT", "LOW_UTIL", "RELIABILITY",
	"6", "7", "8", "9",
	"TOTAL_APP_CLOCKS", "TOTAL_BASE_CLOCKS" };

bool
getViolations(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i;
	int has;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	assert(NVML_PERF_POLICY_COUNT == 12);
	PROM_DEBUG("getViolations", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_VIOL);

	for (i = 0; i < devs; i++) {
		nvmlPerfPolicyType_t policy;
		nvmlViolationTime_t time;

		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasViolation == -1)
			continue;
		has = 0;
		for (policy = 0; policy < NVML_PERF_POLICY_COUNT; policy++) {
			if (policy > 5 && policy < 10)
				continue;
			if (gpu->hasViolation != 0 &&
				(gpu->hasViolation & (1 << policy)) == 0)
			{
				continue;
			}
			res = nvmlDeviceGetViolationStatus(gpu->dev, policy, &time);
			if (NVML_SUCCESS == res) {
				// we do not care about < 5 ms deltas wrt. reference time
				snprintf(buf, sizeof(buf),
					NVMEXM_VIOL_N "{gpu=\"%d\",policy=\"%s\",uuid=\"%s\"} %g\n",
					i, pname[policy], gpu->uuid, time.violationTime * 1e-6);
				psb_add_str(sb, buf);
				PROM_DEBUG("%s = ref = %llu   viol = %llu", pname[policy],
					time.referenceTime, time.violationTime);
				has |= 1 << policy;
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasViolation[%s] = -1", pname[policy]);
			}
		}
		gpu->hasViolation = (has == 0) ? -1 : has;
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
