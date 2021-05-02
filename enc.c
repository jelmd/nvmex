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

#include "enc.h"

static const char *codec[] = { "h264", "hevc", "unknown" };

static void
addSessionInfo(gpu_t *gpu, uint sessions, psb_t *sb_fps, psb_t *sb_lat) {
	nvmlReturn_t res;
	static uint siLen = 0;
	static nvmlEncoderSessionInfo_t *si = NULL;
	nvmlEncoderSessionInfo_t *ni;
	char buf[MBUF_SZ];
	uint c, k;

	if (siLen < sessions) {
		ni = (si == NULL)
			? malloc(sessions * sizeof(nvmlEncoderSessionInfo_t))
			: realloc(si, sessions * sizeof(nvmlEncoderSessionInfo_t));
		if (ni == NULL)
			return;
		si = ni;
		siLen = sessions;
	}
	res = nvmlDeviceGetEncoderSessions(gpu->dev, &sessions, si);
	if (NVML_SUCCESS == res) {
		for (k = 0; k < sessions; k++) {
			c = (si[k].codecType > 1 ) ? 2 : si[k].codecType;
			snprintf(buf, sizeof(buf),
				NVMEXM_ENCSESS_FPS_N "{gpu=\"%d\",codec=\"%s\",hres=\"%u\",vres=\"%u\",sid=\"%u\",pid=\"%u\",vgpu=\"%u\",uuid=\"%s\"} %u\n",
				gpu->idx, codec[c], si[k].hResolution, si[k].vResolution,
			   	si[k].sessionId, si[k].pid, si[k].vgpuInstance, gpu->uuid,
				si[k].averageFps);
			psb_add_str(sb_fps, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_ENCSESS_LAT_N "{gpu=\"%d\",codec=\"%s\",hres=\"%u\",vres=\"%u\",sid=\"%u\",pid=\"%u\",vgpu=\"%u\",uuid=\"%s\"} %u\n",
				gpu->idx, codec[c], si[k].hResolution, si[k].vResolution,
			   	si[k].sessionId, si[k].pid, si[k].vgpuInstance, gpu->uuid,
				si[k].averageLatency);
			psb_add_str(sb_lat, buf);
		}
		gpu->hasEncSessions = 1;
	} else if (NOT_AVAIL(res)) {
		PROM_DEBUG("gpu.hasEncSessions = -1", "");
		gpu->hasEncSessions = -1;
	}
}

bool
getEnc(psb_t *sb, bool compact, uint devs, gpu_t devList[], bool full) {
	nvmlReturn_t res;
	gpu_t *gpu;
	size_t sz;
	uint i, sessions, fps, latency;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
	psb_t *sb_fps, *sb_lat, *sb_sfps, *sb_slat;

	if (devs == 0)
		return false;

	PROM_DEBUG("getEnc", "");
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
		addPromInfo(NVMEXM_ENCSTAT_SESS);
	for (i = 0; i < devs; i++) {
		gpu = &(devList[i]);
		if (gpu->dev == NULL || gpu->hasEncStats == -1)
			continue;
		res = nvmlDeviceGetEncoderStats(gpu->dev, &sessions, &fps, &latency);
		if (NVML_SUCCESS == res) {
			snprintf(buf, sizeof(buf),
				NVMEXM_ENCSTAT_SESS_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, sessions);
			psb_add_str(sb, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_ENCSTAT_FPS_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, fps);
			psb_add_str(sb_fps, buf);
			snprintf(buf, sizeof(buf),
				NVMEXM_ENCSTAT_LAT_N "{gpu=\"%d\",uuid=\"%s\"} %u\n",
				i, gpu->uuid, latency);
			psb_add_str(sb_lat, buf);
			gpu->hasEncStats = 1;
			if (full && sessions > 0 && gpu->hasEncSessions != -1)
				addSessionInfo(gpu, sessions, sb_sfps, sb_slat);
		} else if (NOT_AVAIL(res)) {
			PROM_DEBUG("gpu.hasEncStats = -1", "");
			gpu->hasEncStats = -1;
		}
	}

	if (!compact)
		addPromInfo(NVMEXM_ENCSTAT_FPS);
	psb_add_str(sb, psb_str(sb_fps));

	if (!compact)
		addPromInfo(NVMEXM_ENCSTAT_LAT);
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
