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

#include "nvlink.h"


static uint fields[] = {
	// sed -rne '/^#define NVML_FI_DEV_NVLINK_.*(TOTAL|COUNT|COMMON|TX|RX)[[:space:]]/ { s/^#define[[:space:]]/       /; s|[[:space:]]+([0-9]+)[[:space:]]+|, // \1 | ; s|//\!<[[:space:]]+| | ; p; }' ~/tmp/nv/nvml.h

	// error count requires root access on some platforms
	NVML_FI_DEV_NVLINK_CRC_FLIT_ERROR_COUNT_TOTAL,	// 38  NVLink flow control CRC  Error Counter total for all Lanes
	NVML_FI_DEV_NVLINK_CRC_DATA_ERROR_COUNT_TOTAL,	// 45  NvLink data CRC Error Counter total for all Lanes
	NVML_FI_DEV_NVLINK_REPLAY_ERROR_COUNT_TOTAL,	// 52  NVLink Replay Error Counter total for all Lanes
	NVML_FI_DEV_NVLINK_RECOVERY_ERROR_COUNT_TOTAL,	// 59  NVLink Recovery Error Counter total for all Lanes

#ifdef NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX
	NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX,	// 138  NVLink TX Data throughput in KiB
	NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_RX,	// 139  NVLink RX Data throughput in KiB
	NVML_FI_DEV_NVLINK_THROUGHPUT_RAW_TX,	// 140  NVLink TX Data + protocol overhead in KiB
	NVML_FI_DEV_NVLINK_THROUGHPUT_RAW_RX,	// 141  NVLink RX Data + protocol overhead in KiB
#else
	/* No-one knows, what this means - crap.
	NVML_FI_DEV_NVLINK_BANDWIDTH_C0_TOTAL,	// 66  NVLink Bandwidth Counter Total for Counter Set 0, All Lanes
	NVML_FI_DEV_NVLINK_BANDWIDTH_C1_TOTAL,	// 73  NVLink Bandwidth Counter Total for Counter Set 1, All Lanes
	 */
#endif
};

static const char *fmt[] = {
	NVMEXM_NVLINK_ERR_N "{gpu=\"%d\",type=\"crc-flow-control\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_ERR_N "{gpu=\"%d\",type=\"crc-data\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_ERR_N "{gpu=\"%d\",type=\"replay\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_ERR_N "{gpu=\"%d\",type=\"recovery\",uid=\"%s\"} %llu\n",

	NVMEXM_NVLINK_TRAFFIC_N "{gpu=\"%d\",type=\"data\",value=\"tx\",link=\"all\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_TRAFFIC_N "{gpu=\"%d\",type=\"data\",value=\"rx\",link=\"all\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_TRAFFIC_N "{gpu=\"%d\",type=\"raw\",value=\"tx\",link=\"all\",uid=\"%s\"} %llu\n",
	NVMEXM_NVLINK_TRAFFIC_N "{gpu=\"%d\",type=\"raw\",value=\"rx\",link=\"all\",uid=\"%s\"} %llu\n"
};

#ifndef NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX
static void
initTrafficCounterLegacy(gpu_t *gpu) {
	if (gpu->nvLinks == 0)
		return;

	nvmlReturn_t res;
	nvmlNvLinkUtilizationControl_t ctrl;
	uint k, err = 0;

	ctrl.units = NVML_NVLINK_COUNTER_UNIT_BYTES;
	for (k = 0; k < NVML_NVLINK_MAX_LINKS; k++) {
		ctrl.pktfilter = NVML_NVLINK_COUNTER_PKTFILTER_WRITE;
		res = nvmlDeviceSetNvLinkUtilizationControl(gpu->dev, k, 0, &ctrl, 1);
		if (res != NVML_SUCCESS) {
			if (res == NVML_ERROR_NO_PERMISSION && err == 0) {
				err = 1;
				PROM_WARN("NVlink counter init GPU %u: %s",
					gpu->idx, nverror(res));
			}
		}
		ctrl.pktfilter = NVML_NVLINK_COUNTER_PKTFILTER_READ;
		res = nvmlDeviceSetNvLinkUtilizationControl(gpu->dev, k, 1, &ctrl, 1);
	}
	return;
}

static void
countTxRxLegacy(gpu_t *gpu, psb_t *sb) {
	if (gpu->nvLinks < 1 || gpu->nvLinkSkipTxRx[NVML_NVLINK_MAX_LINKS] == 1)
		return;

	nvmlReturn_t res;
	char buf[MBUF_SZ];
	unsigned long long int rxa, txa, rx, tx;
	uint k, e;

	rxa = txa = 0;
	// sum up all links
	e = 0;
	for (k = 0; k < NVML_NVLINK_MAX_LINKS; k++) {
		if (gpu->nvLinkSkipTxRx[k] == 1)
			continue;
		res = nvmlDeviceGetNvLinkUtilizationCounter(gpu->dev, k, 0, &rx, &tx);
		if (res == NVML_SUCCESS) {
			rxa += rx;
			txa += tx;
			continue;
		}
		gpu->nvLinkSkipTxRx[k] = 1;
		e++;
		if (res == NVML_ERROR_NO_PERMISSION && gpu->nvLinkTxRxError == 0) {
			gpu->nvLinkTxRxError = 1;
			PROM_WARN("NVlink traffic counter GPU %u: %s",
				gpu->idx, nverror(res));
		}
	}
	if (e == NVML_NVLINK_MAX_LINKS) {
		gpu->nvLinkSkipTxRx[NVML_NVLINK_MAX_LINKS] = 1;
		return;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	snprintf(buf, sizeof(buf), fmt[6], gpu->idx, gpu->uuid, rxa);
	psb_add_str(sb, buf);
	snprintf(buf, sizeof(buf), fmt[7], gpu->idx, gpu->uuid, txa);
	psb_add_str(sb, buf);
#pragma GCC diagnostic pop
}

#else
#define initTrafficCounterLegacy(x)
#define countTxRxLegacy(x, y)
#endif

static bool
checkStatic(gpu_t *gpu) {
	if (gpu->nvLinkCount != NULL)
		return true;

	nvmlReturn_t res;
	char buf[MBUF_SZ];
	uint k;

	uint stats[] = {
		// 90  Common NVLink Speed in MBps for active links
		NVML_FI_DEV_NVLINK_SPEED_MBPS_COMMON,
		// 91  Number of NVLinks present on the device
		NVML_FI_DEV_NVLINK_LINK_COUNT
	};
	uint max = sizeof(stats)/sizeof(int);
	nvmlFieldValue_t *fvals = malloc(sizeof(nvmlFieldValue_t) * max);
	if (fvals == NULL)
		return false;
	memset(fvals, 0, sizeof(nvmlFieldValue_t) * max);
	for (k = 0; k < max; k++) {
		fvals[k].fieldId = stats[k];
	}

	res = nvmlDeviceGetFieldValues(gpu->dev, max, fvals);
	if (NVML_SUCCESS == res) {
		if (fvals[0].nvmlReturn == NVML_SUCCESS) {
			snprintf(buf, sizeof(buf), 
				NVMEXM_NVLINK_BW_N "{gpu=\"%d\",uid=\"%s\"} %u\n",
				gpu->idx, gpu->uuid, fvals[0].value.uiVal);
			gpu->nvLinkBW = strdup(buf);
		} else {
			gpu->nvLinkBW = strdup("");
		}
		if (fvals[1].nvmlReturn == NVML_SUCCESS) {
			snprintf(buf, sizeof(buf), 
				NVMEXM_NVLINK_COUNT_N "{gpu=\"%d\",uid=\"%s\"} %u\n",
				gpu->idx, gpu->uuid, fvals[1].value.uiVal);
			gpu->nvLinkCount = strdup(buf);
			gpu->nvLinks = fvals[1].value.uiVal;
		} else {
			gpu->nvLinkCount = strdup("");
			gpu->nvLinks = 0;
		}
	} else if (NOT_AVAIL(res)) {
		gpu->nvLinkBW = strdup("");
		gpu->nvLinkCount = strdup("");
		gpu->nvLinks = 0;
	}
	initTrafficCounterLegacy(gpu);
	return true;
}

bool
getNvLink(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, k, e, max = sizeof(fields)/sizeof(uint);
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
	nvmlFieldValue_t *fvals = NULL;
	psb_t *sb_txrx, *sb_bw, *sb_err;

	if (devs == 0)
		return false;

	sb_txrx = sb_bw = sb_err = NULL;

	PROM_DEBUG("getNvLink", "");

	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	sb_txrx = psb_new();
	sb_bw = psb_new();
	sb_err = psb_new();
	if (sb_txrx == NULL || sb_bw == NULL || sb_err == NULL)
		goto fail;

	fvals = malloc(sizeof(nvmlFieldValue_t) * max);
	if (fvals == NULL)
		goto fail;
	memset(fvals, 0, sizeof(nvmlFieldValue_t) * max);
	for (i = 0; i < max; i++) {
		fvals[i].fieldId = fields[i];
	}

	if (!compact)
		addPromInfo(NVMEXM_NVLINK_COUNT);

	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || !checkStatic(gpu))
			continue;
		psb_add_str(sb, gpu->nvLinkCount);

		if (gpu->nvLinks == 0)
			continue;
		psb_add_str(sb_bw, gpu->nvLinkBW);

		if (gpu->hasNvLinks != -1) {
			res = nvmlDeviceGetFieldValues(gpu->dev, max, fvals);
			if (NVML_SUCCESS == res) {
				e = 0;
				for (k = 0; k < max; k++) {
					if (res != NVML_SUCCESS) {
						e++;
						if (res == NVML_ERROR_NO_PERMISSION
							&& gpu->nvLinkFieldError == 0)
						{
							PROM_WARN("NVlink field[%u] GPU %u: %s",
								fvals[k].fieldId, i, nverror(res));
							gpu->nvLinkFieldError = 1;
						}
						PROM_DEBUG("NVlink field[%u]: %s",
								fvals[k].fieldId, nverror(res));
						continue;
					}
					unsigned long long val; 
#ifdef NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX
					if (fvals[k].fieldId >= NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX
						&& fvals[k].fieldId <= NVML_FI_DEV_NVLINK_THROUGHPUT_RAW_RX)
						val = fvals[k].value.ullVal << 10;
					else
#endif
						val = fvals[k].value.ullVal;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
					snprintf(buf, sizeof(buf), fmt[k], i, gpu->uuid, val);
#pragma GCC diagnostic pop
					if (fvals[k].fieldId <= NVML_FI_DEV_NVLINK_RECOVERY_ERROR_COUNT_TOTAL)
						psb_add_str(sb_err, buf);
					else
						psb_add_str(sb_txrx, buf);
					if (e == max && gpu->hasNvLinks == 0) {
						PROM_DEBUG("gpu.hasNvLinks = -1", "");
						gpu->hasNvLinks = -1;
					}
				}
			} else if (NOT_AVAIL(res)) {
				PROM_DEBUG("gpu.hasNvLinks = -1", "");
				gpu->hasNvLinks = -1;
			}
		}
		countTxRxLegacy(gpu, sb_txrx);
	}

	if (!compact)
		addPromInfo(NVMEXM_NVLINK_BW);
	psb_add_str(sb, psb_str(sb_bw));
	if (!compact)
		addPromInfo(NVMEXM_NVLINK_TRAFFIC);
	psb_add_str(sb, psb_str(sb_txrx));
	if (!compact)
		addPromInfo(NVMEXM_NVLINK_ERR);
	psb_add_str(sb, psb_str(sb_err));

fail:
	psb_destroy(sb_bw);
	psb_destroy(sb_txrx);
	psb_destroy(sb_err);
	free(fvals);

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
