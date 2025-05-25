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

#include "fbc.h"

static const char *stype[] =
	{ "unknown", "tosys", "cuda", "vid", "hwenc", "???" };

// since 10.0
#ifdef NVML_NVFBC_SESSION_FLAG_DIFFMAP_ENABLED
static void
addFbcSessionInfo(gpu_t *gpu, uint sessions, psb_t *sb_fps, psb_t *sb_lat) {
	nvmlReturn_t res;
	static uint siLen = 0;
	static nvmlFBCSessionInfo_t *si = NULL;
	nvmlFBCSessionInfo_t *ni;
	char buf[MBUF_SZ];
	int c;
	uint k;

	if (siLen < sessions) {
		ni = (si == NULL)
			? malloc(sessions * sizeof(nvmlFBCSessionInfo_t))
			: realloc(si, sessions * sizeof(nvmlFBCSessionInfo_t));
		if (ni == NULL)
			return;
		si = ni;
		siLen = sessions;
	}
	res = nvmlDeviceGetFBCSessions(gpu->dev, &sessions, si);
	if (NVML_SUCCESS == res) {
		for (k = 0; k < sessions; k++) {
			c = (si[k].sessionType > 4 ) ? 5 : si[k].sessionType;
			snprintf(buf, sizeof(buf), NVMEXM_FBCSESS_FPS_N
				"{gpu=\"%d\",stype=\"%s\",hres=\"%u\","
				"hresmax=\"%u\",vres=\"%u\",vresmax=\"%u\","
				"sid=\"%u\",sflags=\"%u\",display=\"%u\","
				"pid=\"%u\",vgpu=\"%u\",uuid=\"%s\"} %u\n",
				gpu->idx, stype[c], si[k].hResolution,
				si[k].hMaxResolution, si[k].vResolution, si[k].vMaxResolution,
			   	si[k].sessionId, si[k].sessionFlags, si[k].displayOrdinal,
				si[k].pid, si[k].vgpuInstance, gpu->uuid,
				si[k].averageFPS);
			psb_add_str(sb_fps, buf);
			snprintf(buf, sizeof(buf), NVMEXM_FBCSESS_LAT_N
				"{gpu=\"%d\",stype=\"%s\",hres=\"%u\","
				"hresmax=\"%u\",vres=\"%u\",vresmax=\"%u\","
				"sid=\"%u\",sflags=\"%u\",display=\"%u\","
				"pid=\"%u\",vgpu=\"%u\",uuid=\"%s\"} %u\n",
				gpu->idx, stype[c], si[k].hResolution,
				si[k].hMaxResolution, si[k].vResolution, si[k].vMaxResolution,
			   	si[k].sessionId, si[k].sessionFlags, si[k].displayOrdinal,
				si[k].pid, si[k].vgpuInstance, gpu->uuid,
				si[k].averageLatency);
			psb_add_str(sb_lat, buf);
			gpu->hasFbcSessions = 1;
		}
	} else if (NOT_AVAIL(res)) {
		PROM_DEBUG("gpu.hasFbcSessions = -1", "");
		gpu->hasFbcSessions = -1;
	}
}

bool
getFBC(psb_t *sb, bool compact, uint devs, gpu_t devList[], bool full) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
	psb_t *sb_fps, *sb_lat, *sb_sfps, *sb_slat;

	if (devs == 0)
		return false;

	PROM_DEBUG("getFB", "");
	if (free_sb)
		sb = psb_new();
	sz = psb_len(sb);

	sb_fps = psb_new();
	sb_lat = psb_new();
	sb_sfps = full ? psb_new() : NULL;
	sb_slat = full ? psb_new() : NULL;
	if (sb_fps == NULL || sb_lat == NULL
		|| (full && (sb_sfps == NULL || sb_slat == NULL)))
	{
		goto end;
	}

	if (!compact)
		addPromInfo(NVMEXM_FBCSTAT_SESS);

	for (i = 0; i < devs; i++) {
		nvmlFBCStats_t stats;
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasFbcStats == -1)
			continue;
		res = nvmlDeviceGetFBCStats(gpu->dev, &stats);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_FBCSTAT_SESS_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, stats.sessionsCount);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_FBCSTAT_FPS_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, stats.averageFPS);
			psb_add_str(sb_fps, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_FBCSTAT_LAT_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, stats.averageLatency);
			psb_add_str(sb_lat, buf);
			gpu->hasFbcStats = 1;
			if (full && stats.sessionsCount > 0 && gpu->hasFbcSessions != -1)
				addFbcSessionInfo(gpu, stats.sessionsCount, sb_sfps, sb_slat);
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasFbcStats = -1", "");
			gpu->hasFbcStats = -1;
		}
	}

	if (!compact)
		addPromInfo(NVMEXM_FBCSTAT_FPS);
	psb_add_str(sb, psb_str(sb_fps));

	if (!compact)
		addPromInfo(NVMEXM_FBCSTAT_LAT);
	psb_add_str(sb, psb_str(sb_lat));

	sz = psb_len(sb) - sz;
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}

end:
	psb_destroy(sb_fps);
	psb_destroy(sb_lat);
	psb_destroy(sb_sfps);
	psb_destroy(sb_slat);

	return sz != 0;
}
#else
#pragma message "Skipping 'nvmlDeviceGetFBCStats()' support."
#pragma message "Skipping 'nvmlDeviceGetFBCSessions()' support."
bool
getFBC(psb_t *sb, bool compact, uint devs, gpu_t devList[], bool full) {
	return false;
}
#endif /* NVML_NVFBC_SESSION_FLAG_DIFFMAP_ENABLED */
