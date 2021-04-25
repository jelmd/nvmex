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
#include <string.h>

#include "temperature.h"

static void
setStaticValues(gpu_t *gpu) {
	nvmlReturn_t res;
	char buf[MBUF_SZ * 4];
	uint temp;
	int i;
	size_t len;

	assert(NVML_TEMPERATURE_THRESHOLD_COUNT == 4);
	int order[] = {
		NVML_TEMPERATURE_THRESHOLD_GPU_MAX,
		NVML_TEMPERATURE_THRESHOLD_SLOWDOWN,
		NVML_TEMPERATURE_THRESHOLD_SHUTDOWN,
		NVML_TEMPERATURE_THRESHOLD_MEM_MAX
	};

	const char *name[] = {
		"device=\"gpu\",value=\"max\"",
		"device=\"gpu\",value=\"slowdown\"",
		"device=\"gpu\",value=\"shutdown\"",
		"device=\"mem\",value=\"max\""
	};
	buf[0] = '\0';
	len = 0;
	for (i = 0; i < 4 && len < sizeof(buf); i++) {
		res = nvmlDeviceGetTemperatureThreshold(gpu->dev, order[i], &temp);
		if (NVML_SUCCESS == res) {
			len += snprintf(buf + len, sizeof(buf) - len,
				NVMEXM_TEMPERATURE_N "{gpu=\"%d\",%s,uuid=\"%s\"} %u\n",
				gpu->idx, name[i], gpu->uuid, temp);
		} else {
			PROM_DEBUG("No %s{gpu=\"%d\",%s}",
				NVMEXM_TEMPERATURE_N, gpu->idx, name[i]);
		}
	}
	gpu->temperatures = strdup(buf);
}

bool
getTemperatures(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, value;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
	nvmlFieldValue_t fval;

	if (devs == 0)
		return false;

	PROM_DEBUG("getTemperatures", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_TEMPERATURE);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasTemperature == -1)
			continue;
		res = nvmlDeviceGetTemperature(gpu->dev, NVML_TEMPERATURE_GPU, &value);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_TEMPERATURE_N
				"{gpu=\"%d\",device=\"gpu\",value=\"now\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, value);
			psb_add_str(sb, buf);
			gpu->hasTemperature = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasTemperature = -1", "");
			gpu->hasTemperature = -1;
		}
		/* there seems to be no API to get memory temperature, except generic */
		if (gpu->hasTemperature != -1) {
			memset(&fval, 0, sizeof(fval));	// make sure unused|scopeId == 0
			fval.fieldId = NVML_FI_DEV_MEMORY_TEMP;
			res = nvmlDeviceGetFieldValues(gpu->dev, 1, &fval);
			if (NVML_SUCCESS == res && fval.value.uiVal != 0) {
				snprintf(buf, sizeof(buf), NVMEXM_TEMPERATURE_N
					"{gpu=\"%d\",device=\"mem\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, value);
				psb_add_str(sb, buf);
				gpu->hasTemperatureMem = 1;
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasTemperatureMem = -1", "");
				gpu->hasTemperature = -1;
			}
		}
		if (gpu->temperatures == NULL)
			setStaticValues(gpu);
		psb_add_str(sb, gpu->temperatures);
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
