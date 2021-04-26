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

#include "ecc.h"

// cat nvml.h | gsed -rne '/^#define NVML_FI_DEV_ECC_/ { s/^#define NVML_FI_DEV_ECC_/	"/; s|[[:space:]] *([0-9]+)[[:space:]]+//!<|",	// \1 |; s/ (single|double).*//; s/CBU/Convergence Barrier Unit/; s/TOTAL/ALL/; p; }'

#define _OFFSET 3

static const char *ename[] = {
	// SBE|DBE	single|double bit errors
	// VOL|AGG	volatile (since last modul load/reset)|persistent counter
	"SBE_VOL_ALL",	// 3  Total
	"DBE_VOL_ALL",	// 4  Total
	"SBE_AGG_ALL",	// 5  Total
	"DBE_AGG_ALL",	// 6  Total
	"SBE_VOL_L1",	// 7  L1 cache
	"DBE_VOL_L1",	// 8  L1 cache
	"SBE_VOL_L2",	// 9  L2 cache
	"DBE_VOL_L2",	// 10  L2 cache
	"SBE_VOL_DEV",	// 11  Device memory
	"DBE_VOL_DEV",	// 12  Device memory
	"SBE_VOL_REG",	// 13  Register file
	"DBE_VOL_REG",	// 14  Register file
	"SBE_VOL_TEX",	// 15  Texture memory
	"DBE_VOL_TEX",	// 16  Texture memory
	"DBE_VOL_CBU",	// 17  Convergence Barrier Unit
	"SBE_AGG_L1",	// 18  L1 cache
	"DBE_AGG_L1",	// 19  L1 cache
	"SBE_AGG_L2",	// 20  L2 cache
	"DBE_AGG_L2",	// 21  L2 cache
	"SBE_AGG_DEV",	// 22  Device memory
	"DBE_AGG_DEV",	// 23  Device memory
	"SBE_AGG_REG",	// 24  Register File
	"DBE_AGG_REG",	// 25  Register File
	"SBE_AGG_TEX",	// 26  Texture memory
	"DBE_AGG_TEX",	// 27  Texture memory
	"DBE_AGG_CBU"	// 28  Convergence Barrier Unit
};

bool
getECC(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, k, max;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
	nvmlFieldValue_t *fvals = NULL;
	psb_t *sbe, *sbp, *sbr; // buffer for error + retired pages + remapped rows
   	sbe = sbp = sbr = NULL;
	nvmlEnableState_t state;

	if (devs == 0)
		return false;

	PROM_DEBUG("getECC", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	if (!compact)
		addPromInfo(NVMEXM_ECC_MODE);

	for (i = 0; i < devs; i++) {
		nvmlEnableState_t current = 0;

		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasECC == -1)
			continue;

		// mode
		res = nvmlDeviceGetEccMode(gpu->dev, &current, &state);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_ECC_MODE_N
				"{gpu=\"%d\",mode=\"current\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, current);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf), NVMEXM_ECC_MODE_N
				"{gpu=\"%d\",mode=\"pending\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, state);
			psb_add_str(sb, buf);
			gpu->hasECC = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasECC = -1", "");
			gpu->hasECC = -1;
		}
		if (current == 0)
			continue;

		// errors
		max = sizeof(ename)/sizeof(char *);
		if (fvals == NULL) {
			fvals = malloc(sizeof(nvmlFieldValue_t) * max);
			if (fvals == NULL)
				continue;
			memset(fvals, 0, sizeof(nvmlFieldValue_t) * max);
		}
		if (sbe == NULL) {
			sbe = psb_new();
			if (sbe == NULL)
				continue;
		}
		for (k = 0; k < max; k++)
			fvals[k].fieldId = k + _OFFSET;
		res = nvmlDeviceGetFieldValues(gpu->dev, max, fvals);
		if (NVML_SUCCESS != res)
			continue;
		for (k = 0; k < max; k++) {
			if (fvals[k].nvmlReturn != NVML_SUCCESS)
				continue;
			snprintf(buf, sizeof(buf), NVMEXM_ECC_ERR_N
				"{gpu=\"%d\",type=\"%s\",counter=\"%s\",loc=\"%s\",uuid=\"%s\"}"
				//" %d"
			    " %llu\n",
				i, (ename[k][0] == 'S' ? "sbe" : "dbe"),
				(ename[k][4] == 'V' ? "volatile" : "persistent"),
				ename[k] + 8,
				gpu->uuid,
				//fvals[k].valueType,
				fvals[k].value.ullVal);
			psb_add_str(sbe, buf);
		}
	}

	free(fvals);
	if (sbe != NULL) {
		addPromInfo(NVMEXM_ECC_ERR);
		psb_add_str(sb, psb_str(sbe));
		psb_destroy(sbe);
	}

	// hmm, if ECC is disabled, this seems to be useless ...
	if (!compact)
		addPromInfo(NVMEXM_ECC_PAGE);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasRetiredPages == -1)
			continue;
		k = 0;
		res = nvmlDeviceGetRetiredPages(gpu->dev,
			NVML_PAGE_RETIREMENT_CAUSE_MULTIPLE_SINGLE_BIT_ECC_ERRORS, &k,NULL);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_ECC_PAGE_N
				"{gpu=\"%d\",type=\"sbe\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, k);
			psb_add_str(sb, buf);
			gpu->hasRetiredPages = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasRetiredPages = -1", "");
			gpu->hasRetiredPages = -1;
			continue;
		}
		res = nvmlDeviceGetRetiredPages(gpu->dev,
			NVML_PAGE_RETIREMENT_CAUSE_DOUBLE_BIT_ECC_ERROR, &k, NULL);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_ECC_PAGE_N
				"{gpu=\"%d\",type=\"dbe\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, k);
			psb_add_str(sb, buf);
		}
		res = nvmlDeviceGetRetiredPagesPendingStatus(gpu->dev, &state);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_ECC_PAGE_N
				"{gpu=\"%d\",type=\"pending\",uuid=\"%s\"} %d\n",
				i, gpu->uuid, state);
			psb_add_str(sb, buf);
		}
	}

#ifdef NVML_FI_DEV_REMAPPED_COR
	if (!compact)
		addPromInfo(NVMEXM_ECC_ROW);
	for (i = 0; i < devs; i++) {
		uint e, p;

		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasRemappedRows == -1)
			continue;
		res = nvmlDeviceGetRemappedRows(gpu->dev, &k, &max, &p, &e);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf), NVMEXM_ECC_ROW_N
				"{gpu=\"%d\",type=\"uncorrectable\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, k);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf), NVMEXM_ECC_ROW_N
				"{gpu=\"%d\",type=\"correctable\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, max);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf), NVMEXM_ECC_ROW_N
				"{gpu=\"%d\",type=\"pending\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, p);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf), NVMEXM_ECC_ROW_N
				"{gpu=\"%d\",type=\"failure\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, e);
			psb_add_str(sb, buf);
			gpu->hasRemappedRows = 1;
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasRetiredPages = -1", "");
			gpu->hasRemappedRows = -1;
		}
	}
#else
#pragma message "Skipping 'nvmlDeviceGetRemappedRows()' support."
#endif

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return sz != 0;
}
