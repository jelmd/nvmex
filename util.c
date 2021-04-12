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

#include "util.h"

bool
getUtilization(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz, len;
	uint i;
	char buf[MBUF_SZ * 2];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getUtilization", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_UTIL);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL)
			continue;
		if (gpu->hasUtil != -1) {
			nvmlUtilization_t percent;
			res = nvmlDeviceGetUtilizationRates(gpu->dev, &percent);
			if (NVML_SUCCESS == res) {
				snprintf(buf, sizeof(buf),
				NVMEXM_UTIL_N "{gpu=\"%d\",dev=\"gpu\",uuid=\"%s\"} %u\n"
				NVMEXM_UTIL_N "{gpu=\"%d\",dev=\"memory\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, percent.gpu,
					i, gpu->uuid, percent.memory);
				psb_add_str(sb, buf);
				gpu->hasUtil = 1;
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasUtil = -1", "");
				gpu->hasUtil = -1;
			}
		}
		if (gpu->hasDecoderUtil != -1) {
			uint percent, period;
			res = nvmlDeviceGetDecoderUtilization(gpu->dev, &percent, &period);
			if (NVML_SUCCESS == res) {
				len = compact ? 0 : snprintf(buf, sizeof(buf),
					"#  sample interval: %u ms\n", period);
				snprintf(buf + len, sizeof(buf) - len,
				NVMEXM_UTIL_N "{gpu=\"%d\",dev=\"decoder\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, percent);
				psb_add_str(sb, buf);
				gpu->hasDecoderUtil = 1;
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasDecoderUtil = -1", "");
				gpu->hasDecoderUtil = -1;
			}
			res = nvmlDeviceGetEncoderUtilization(gpu->dev, &percent, &period);
			if (NVML_SUCCESS == res) {
				len = compact ? 0 : snprintf(buf, sizeof(buf),
					"#  sample interval: %u ms\n", period);
				snprintf(buf + len, sizeof(buf) - len,
				NVMEXM_UTIL_N "{gpu=\"%d\",dev=\"encoder\",uuid=\"%s\"} %u\n",
					i, gpu->uuid, percent);
				psb_add_str(sb, buf);
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasEncoderUtil = -1", "");
			}
		}
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
