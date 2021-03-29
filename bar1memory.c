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

#include "bar1memory.h"

bool
getBar1memory(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	nvmlBAR1Memory_t mem;
	gpu_t gpu;
	size_t sz;
	uint i;
	char buf[MBUF_SZ * 3];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getBar1memory", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_BAR1MEM);
	for (i = 0; i < devs; i++) {
		gpu = devList[i];
		if (gpu.dev == NULL || gpu.hasBar1memory == -1)
			continue;
		res = nvmlDeviceGetBAR1MemoryInfo(gpu.dev, &mem);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_BAR1MEM_N "{gpu=\"%d\",sz=\"free\",uuid=\"%s\"} %lld\n"
				NVMEXM_BAR1MEM_N "{gpu=\"%d\",sz=\"total\",uuid=\"%s\"} %lld\n"
				NVMEXM_BAR1MEM_N "{gpu=\"%d\",sz=\"used\",uuid=\"%s\"} %lld\n",
				i, gpu.uuid, mem.bar1Free,
				i, gpu.uuid, mem.bar1Total,
				i, gpu.uuid, mem.bar1Used);
			psb_add_str(sb, buf);
			gpu.hasBar1memory = 1;
		} else if (NOT_AVAIL(res)) {
			gpu.hasBar1memory = -1;
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
