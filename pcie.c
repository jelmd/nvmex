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

#include "pcie.h"

static int
setLinkInfo(gpu_t *gpu) {
	nvmlReturn_t res;
	char buf[MBUF_SZ * 4];
	size_t len = 0;
	uint val;

	buf[0] = '\0';
	res = nvmlDeviceGetCurrPcieLinkGeneration(gpu->dev, &val);
	if (NVML_SUCCESS == res) {
		len = snprintf(buf, sizeof(buf),
			NVMEXM_PCIE_LINK_N "{gpu=\"%d\",value=\"Gen\",uuid=\"%s\"} %u\n",
			gpu->idx, gpu->uuid, val);
	}
	res = nvmlDeviceGetMaxPcieLinkGeneration (gpu->dev, &val);
	if (NVML_SUCCESS == res) {
		len += snprintf(buf + len, sizeof(buf) - len,
			NVMEXM_PCIE_LINK_N "{gpu=\"%d\",value=\"maxGen\",uuid=\"%s\"} %u\n",
			gpu->idx, gpu->uuid, val);
	}
	res = nvmlDeviceGetCurrPcieLinkWidth(gpu->dev, &val);
	if (NVML_SUCCESS == res) {
		len += snprintf(buf + len, sizeof(buf) - len,
			NVMEXM_PCIE_LINK_N "{gpu=\"%d\",value=\"Width\",uuid=\"%s\"} %u\n",
			gpu->idx, gpu->uuid, val);
	}
	res = nvmlDeviceGetMaxPcieLinkWidth(gpu->dev, &val);
	if (NVML_SUCCESS == res) {
		len += snprintf(buf + len, sizeof(buf) - len,
			NVMEXM_PCIE_LINK_N "{gpu=\"%d\",value=\"Width\",uuid=\"%s\"} %u\n",
			gpu->idx, gpu->uuid, val);
	}
	gpu->pcieLinkInfo = strdup(buf);
	return len == 0 ? 0 : 1;
}

bool
getPCIe(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res, res2;
	gpu_t *gpu;
	size_t sz;
	uint i, v, w, c = 0;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	if (devs == 0)
		return false;

	PROM_DEBUG("getPCIe", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_PCIE_UTIL);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasPCIeUtil == -1)
			continue;
		res = nvmlDeviceGetPcieThroughput(gpu->dev, NVML_PCIE_UTIL_TX_BYTES,&v);
		res2 = nvmlDeviceGetPcieThroughput(gpu->dev,NVML_PCIE_UTIL_RX_BYTES,&w);
		if (NVML_SUCCESS == res && NVML_SUCCESS == res2) {
			snprintf(buf, sizeof(buf),
				NVMEXM_PCIE_UTIL_N "{gpu=\"%d\",value=\"tx\",uuid=\"%s\"} %ld\n",
				i, gpu->uuid, v * 1000L);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_PCIE_UTIL_N "{gpu=\"%d\",value=\"rx\",uuid=\"%s\"} %ld\n",
				i, gpu->uuid, w * 1000L);
			psb_add_str(sb, buf);
			gpu->hasPCIeUtil = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasPCIeUtil = -1", "");
			gpu->hasPCIeUtil = -1;
		}
		c += (gpu->pcieLinkInfo == NULL)
			? setLinkInfo(gpu)
			: (gpu->pcieLinkInfo[0] == '\0' ? 0 : 1);
	}

	if (!compact)
		addPromInfo(NVMEXM_PCIE_REPLAY);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasPCIeReplay == -1)
			continue;
		res = nvmlDeviceGetPcieReplayCounter(gpu->dev, &v);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_PCIE_REPLAY_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, v);
			psb_add_str(sb, buf);
			gpu->hasPCIeReplay = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasPCIeReplay = -1", "");
			gpu->hasPCIeReplay = -1;
		}
	}

	if (c > 0) {
		if (!compact)
			addPromInfo(NVMEXM_PCIE_LINK);
		for (i = 0; i < devs; i++)
			psb_add_str(sb, devList[i].pcieLinkInfo);
	}

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
