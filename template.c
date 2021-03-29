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

#include "XXX.h"

bool
getXXX(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getXXX", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_XXX);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasXXX == -1)
			continue;
		res = nvml(gpu->dev, ...);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_XXX_N "{gpu=\"%d\",DDDDD=\"EEEE\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, FFFFF);
			psb_add_str(sb, buf);
			gpu->hasXXX = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasXXX = -1", "");
			gpu->hasXXX = -1;
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
