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

#include <string.h>

#include "power.h"

static void
setLimits(gpu_t *gpu) {
	nvmlReturn_t res;
	uint limit, limitb;
	char buf[MBUF_SZ * 2];
	size_t len = 0;

	buf[0] = '\0';
	// power limit when booting
	res = nvmlDeviceGetPowerManagementDefaultLimit(gpu->dev, &limit);
	if (NVML_SUCCESS == res) {
		snprintf(buf, sizeof(buf), NVMEXM_POWER_N
			"{gpu=\"%d\",limit=\"default\",uuid=\"%s\"} %u\n",
			gpu->idx, gpu->uuid, limit);
	} else {
		PROM_DEBUG("No %s{gpu=\"%d\",limit=\"default\"}", NVMEXM_POWER_N,
		   gpu->idx);
	}
	// min/max limits
	res = nvmlDeviceGetPowerManagementLimitConstraints(gpu->dev,&limit,&limitb);
	if (NVML_SUCCESS == res) {
		snprintf(buf + len, sizeof(buf) - len,
			NVMEXM_POWER_N "{gpu=\"%d\",limit=\"min\",uuid=\"%s\"} %u\n"
			NVMEXM_POWER_N "{gpu=\"%d\",limit=\"max\",uuid=\"%s\"} %u\n" ,
			gpu->idx, gpu->uuid, limit,
			gpu->idx, gpu->uuid, limitb);
	} else {
		PROM_DEBUG("No %s{gpu=\"%d\",limit=\"min/max\"}", NVMEXM_POWER_N,
			gpu->idx);
	}
	gpu->powerlimits = strdup(buf);
}

bool
getPower(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, power;
	unsigned long long mj;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getPower", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_POWER_CONSUM);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasPowerConsum == -1)
			continue;
		res = nvmlDeviceGetTotalEnergyConsumption(gpu->dev, &mj);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_POWER_CONSUM_N "{gpu=\"%d\",uuid=\"%s\"} %lld\n",
				i, gpu->uuid, mj);
			psb_add_str(sb, buf);
			gpu->hasPowerConsum = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasPowerConsum = -1", "");
			gpu->hasPowerConsum = -1;
		}
	}

	if (!compact)
		addPromInfo(NVMEXM_PSTATE);
	for (i = 0; i < devs; i++) {
		nvmlPstates_t state;
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasPstate == -1)
			continue;
		res = nvmlDeviceGetPerformanceState(gpu->dev, &state);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_PSTATE_N "{gpu=\"%d\",uuid=\"%s\"} %d\n",
				i, gpu->uuid, state);
			psb_add_str(sb, buf);
			gpu->hasPstate = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasPstate = -1", "");
			gpu->hasPstate = -1;
		}
	}

	if (!compact)
		addPromInfo(NVMEXM_POWER);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL)
			continue;
		if (gpu->hasPower != -1) {
			res = nvmlDeviceGetPowerUsage(gpu->dev, &power);
			if (NVML_SUCCESS == res) {
				snprintf(buf, sizeof(buf),
					NVMEXM_POWER_N"{gpu=\"%d\",usage=\"now\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, power);
				psb_add_str(sb, buf);
				gpu->hasPower = 1;
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasPower = -1", "");
				gpu->hasPower = -1;
			}
		}
		if (gpu->hasPowerLimit != -1) {
			// final decision
			res = nvmlDeviceGetEnforcedPowerLimit(gpu->dev, &power);
			if (NVML_SUCCESS == res) {
				snprintf(buf, sizeof(buf), NVMEXM_POWER_N
					"{gpu=\"%d\",limit=\"enforced\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, power);
				psb_add_str(sb, buf);
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("No %s{gpu=\"%d\",limit=\"enforced\"}",
					NVMEXM_POWER_N, i);
			}
			// when  power management algorithm kicks in - settable, so static
			// but not really.
			res = nvmlDeviceGetPowerManagementLimit(gpu->dev, &power);
			if (NVML_SUCCESS == res) {
				snprintf(buf, sizeof(buf), NVMEXM_POWER_N
					"{gpu=\"%d\",limit=\"throttle\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, power);
				psb_add_str(sb, buf);
				gpu->hasPowerLimit = 1;
			} else {
				PROM_DEBUG("No %s{gpu=\"%d\",limit=\"throttle\"}",
					NVMEXM_POWER_N, gpu->idx);
				gpu->hasPowerLimit = -1;
			}
			if (gpu->powerlimits == NULL)
				setLimits(gpu);
			psb_add_str(sb, gpu->powerlimits);
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
