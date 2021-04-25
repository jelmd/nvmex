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

#include "memory.h"

bool
getMemory(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz, len;
	uint i;
	char buf[MBUF_SZ * 3];
	bool free_sb = sb == NULL;
	nvmlMemory_t memory;

	if (devs == 0)
		return false;

	PROM_DEBUG("getMemory", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_MEM);

	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL)
			continue;
		res = nvmlDeviceGetMemoryInfo(gpu->dev, &memory);
		if (NVML_SUCCESS == res) {
			len = snprintf(buf, sizeof(buf),
				NVMEXM_MEM_N "{gpu=\"%d\",value=\"total\",uuid=\"%s\"} %llu\n",
				i, gpu->uuid, memory.total);
			len += snprintf(buf + len , sizeof(buf) - len,
				NVMEXM_MEM_N "{gpu=\"%d\",value=\"free\",uuid=\"%s\"} %llu\n",
				i, gpu->uuid, memory.free);
			len += snprintf(buf + len, sizeof(buf) - len,
				NVMEXM_MEM_N "{gpu=\"%d\",value=\"used\",uuid=\"%s\"} %llu\n",
				i, gpu->uuid, memory.used);
			psb_add_str(sb, buf);
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
