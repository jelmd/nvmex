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

#include "fan.h"

bool
getFan(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, speed;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getFan", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_FAN);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasFan == -1)
			continue;
		res = nvmlDeviceGetFanSpeed(gpu->dev, &speed);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_FAN_N "{gpu=\"%d\",value=\"intended\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, speed);
			psb_add_str(sb, buf);
			gpu->hasFan = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasFan = -1", "");
			gpu->hasFan = -1;
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
