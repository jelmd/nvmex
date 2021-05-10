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

#include "inspect.h"

/* nvmlInit_v2() already called */
static uint started = 0;
static char *versionHR = NULL;
static char *versionProm = NULL;
static char *gpuInfoHR = NULL;

uint
start(void) {
	if (started)
		return 0;

	nvmlReturn_t res = nvmlInit_v2();
	if (res == NVML_SUCCESS) {
		PROM_DEBUG("NVML initialized", "");
		started = 1;
		return 0;
	}
	PROM_WARN("Failed to initialize NVML: %s", nverror(res));
	return 1;
}

uint
stop(void) {
	nvmlReturn_t res = nvmlShutdown();

	if (res == NVML_SUCCESS) {
		PROM_DEBUG("NVML has been properly shut down", "");
		started = 1;
		return 0;
	}
	PROM_WARN("Failed to shutdown NVML: %s\n", nverror(res));
	return 1;
}

char *
getDevInfos(psb_t *sb, bool compact, uint devs, gpu_t devList[]) {
	nvmlReturn_t res;
	gpu_t *gpu;
	uint i;
	char buf[MBUF_SZ * 4], *p;
	bool free_sb = sb == NULL;
	psb_t *sbi = NULL;

	if (devs == 0)
		return NULL;

	PROM_DEBUG("getDevInfos", "");
	if (free_sb) {
		sb = psb_new();
		if (sb == NULL)
			return NULL;
	}
	if (gpuInfoHR == NULL) {
		sbi = psb_new();
		if (sbi == NULL)
			return NULL;
	}

	if (!compact)
		addPromInfo(NVMEXM_GPU);

	for (i = 0; i < devs; i++) {
		char name[NVML_DEVICE_NAME_BUFFER_SIZE];
		nvmlPciInfo_t pci;

		gpu = &(devList[i]);
		if (gpu->dev == NULL)
			continue;
		if (gpu->info != NULL) {
			psb_add_str(sb, gpu->info);
			continue;
		}
		p = buf;
		buf[0] = '\0';

		// see also  nvmlDeviceGetArchitecture(dev, &arch): maps arch to string
		res = nvmlDeviceGetName(gpu->dev, name, NVML_DEVICE_NAME_BUFFER_SIZE);
		if (NVML_SUCCESS != res) { 
			PROM_WARN("Failed to get name of device %u: %s\n", i, nverror(res));
			name[0] = '\0';
        } else {
			p += snprintf(buf, sizeof(buf),
				NVMEXM_GPU_N "{gpu=\"%d\",name=\"%s\",uuid=\"%s\"} 1\n",
				i, name, gpu->uuid);
		}
		res = nvmlDeviceGetPciInfo(gpu->dev, &pci);
		if (NVML_SUCCESS != res) { 
			PROM_WARN("Failed to get pciInfo of device %u: %s\n", i,
				nverror(res));
			pci.busId[0] = '\0';
		} else {
			p += snprintf(p, sizeof(buf) - (p - buf),
				NVMEXM_GPU_N "{gpu=\"%d\",pci=\"%s\",uuid=\"%s\"} 1\n",
				i, pci.busId, gpu->uuid);
		}
		psb_add_str(sb, buf);
		gpu->info = strdup(buf);
		if (sbi != NULL) {
			nvmlComputeMode_t compute_mode;
			res = nvmlDeviceGetComputeMode(gpu->dev, &compute_mode);
			snprintf(buf, MBUF_SZ, "%u. %s [%s]   %sCUDA capable\n",
				i, name, pci.busId, NVML_ERROR_NOT_SUPPORTED == res ?"not ":"");
			psb_add_str(sbi, buf);
		}
	}
	if (sbi != NULL) {
		gpuInfoHR = psb_dump(sbi);
		psb_destroy(sbi);
	}
	if (free_sb) {
		fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return gpuInfoHR;
}

// System Queries 2.12
char *
getVersions(psb_t *sbp, bool compact) {
	nvmlReturn_t res;
	int v, k, l;
	char buf[MBUF_SZ];
	psb_t *sbi = NULL, *sb = NULL;

	if (versionProm != NULL)
		goto end;

	sbi = psb_new();
	sb = psb_new();
	if (sbi == NULL || sb == NULL) {
		psb_destroy(sbi);
		psb_destroy(sb);
		return NULL;
	}

	if (!compact)
		addPromInfo(NVMEXM_VERS);

	res = nvmlSystemGetCudaDriverVersion(&v);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get CUDA driver version: %s\n", nverror(res));
	} else {
		int min = v%1000;
		snprintf(buf, MBUF_SZ, "CUDA driver version: %d.%d.%d\n",
			v/1000, min/10, min%10);
		psb_add_str(sbi, buf);
		snprintf(buf, MBUF_SZ, NVMEXM_VERS_N
			"{name=\"CUDA driver\",value=\"%d.%d.%d\"} %d\n",
			v/1000, min/10, min%10, v);
		psb_add_str(sb, buf);
	}

	res = nvmlSystemGetDriverVersion (buf, MBUF_SZ);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get System driver version: %s\n", nverror(res));
	} else {
		psb_add_str(sbi, "Kernel module version: ");
		psb_add_str(sbi, buf);
		psb_add_char(sbi, '\n');
		psb_add_str(sb, NVMEXM_VERS_N "{name=\"kernel module\",value=\"");
		psb_add_str(sb, buf);
		psb_add_str(sb, "\"} ");
		l = -1;
		for (k = strlen(buf) -1; k != 0; k--)
			if (buf[k] == '.') {
				buf[k] = '0';
				l = k;
			}
		if (l != -1)
			buf[l] = '.';
		psb_add_str(sb, buf);
		psb_add_char(sb, '\n');
	}

	res = nvmlSystemGetNVMLVersion (buf, MBUF_SZ);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get NVML version: %s\n", nverror(res));
	} else {
		psb_add_str(sbi, "NVML version: ");
		psb_add_str(sbi, buf);
		psb_add_char(sbi, '\n');
		psb_add_str(sb, NVMEXM_VERS_N "{name=\"NVML\",value=\"");
		psb_add_str(sb, buf);
		psb_add_str(sb, "\"} ");
		l = -1;
		for (k = strlen(buf) -1; k != 0; k--)
			if (buf[k] == '.') {
				buf[k] = '0';
				l = k;
			}
		if (l != -1)
			buf[l] = '.';
		psb_add_str(sb, buf);
		psb_add_char(sb, '\n');
	}

	versionHR = psb_dump(sbi);
	psb_destroy(sbi);
	versionProm = psb_dump(sb);
	psb_destroy(sb);

end:
	if (sbp == NULL) {
		fprintf(stdout, "\n%s", versionProm);
	} else {
		psb_add_str(sbp, versionProm);
	}
	return versionHR;
}

// Unit Queries 2.13
uint
getUnitInfos(psb_t *sb) {
	bool free_sb = sb == NULL;
	nvmlReturn_t res;
	//char buf[MBUF_SZ];
	uint units;

	res = nvmlUnitGetCount(&units);
	if (NVML_SUCCESS != res) { 
		PROM_WARN("Failed to get unit count: %s", nverror(res));
		return 0;
	}
	if (units == 0)
		return 0;
	PROM_WARN("Found %d units - give me a call.", units);

	/* Skip, 'til nvidia comes up with an explaination, what "units" and
	   "S-class {systems|products}" are.
	*/
	if (free_sb)
		sb = psb_new();

	if (free_sb) {
		//fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return 0;
}

// Device Queries 2.14
uint
getDevices(gpu_t *gpuList[]) {
	nvmlReturn_t res;
	uint devs, i, count = 0, k;
	nvmlDevice_t *devList;
	char buf[MBUF_SZ];
	size_t bytes;

	// Counts all devices, even those NVML has no permission to talk to.
	res = nvmlDeviceGetCount_v2(&devs);
	if (NVML_SUCCESS != res) { 
		PROM_WARN("Failed to get device count: %s", nverror(res));
		return 0;
	}
    PROM_DEBUG("Found %u device%s", devs, devs != 1 ? "s" : "");
	if (devs == 0) {
		devList = NULL;
		return 0;
	}

	bytes = sizeof(nvmlDevice_t) * devs;
	devList = (nvmlDevice_t *) malloc(bytes);
	if (devList == NULL) {
		PROM_ERROR("Unable to malloc %d bytes", bytes);
		return 0;
	}

	PROM_DEBUG("devList = %p", devList);
	for (i = 0; i < devs; i++) {
		res = nvmlDeviceGetHandleByIndex_v2(i, &(devList[i]));
		if (NVML_SUCCESS != res) { 
			PROM_WARN("Failed to get device handle %u: %s", i, nverror(res));
			devList[i] = NULL;
		} else {
			PROM_DEBUG("gpu[%u].dev = %p", i, devList[i]);
			count++;
		}
	}
	if (count == 0)
		goto end;

	bytes = sizeof(gpu_t) * count;
	*gpuList = malloc(bytes);
	if (*gpuList == NULL)
		goto end;
	memset(*gpuList, 0, bytes);
	PROM_DEBUG("gpuList = %p", *gpuList);

	k = 0;
	for (i = 0; i < devs; i++) {
		if (devList[i] == NULL)
			continue;
		PROM_DEBUG("gpu[%u] = %p", i, &((*gpuList)[i]));
		(*gpuList)[k].dev = devList[i];
		(*gpuList)[k].idx = i;
		res = nvmlDeviceGetUUID((*gpuList)[k].dev, buf, MBUF_SZ);
		if (NVML_SUCCESS != res) {
			PROM_WARN("Failed to get UUID dev %u: %s", i, nverror(res));
		} else {
			(*gpuList)[k].uuid = strdup(buf + 4);	// now it is a propper UUID
		}
		k++;
	}

end:
	free(devList);
	devList = NULL;

	return devs;
}

uint
cleanup(uint devs, gpu_t *devList[]) {
	uint i, k;

	free(versionHR);
	versionHR = NULL;
	free(versionProm);
	versionProm = NULL;
	free(gpuInfoHR);
	gpuInfoHR = NULL;

	if (*devList == NULL || devs == 0)
		return 0;

	for (i = 0; i < devs; i++) {
		free((*devList)[i].uuid);
		free((*devList)[i].pciId);
		free((*devList)[i].info);
		if ((*devList)[i].minMaxClock != NULL) {
			for (k = 0; k < NVML_CLOCK_COUNT; k++)
				free((*devList)[i].minMaxClock[k]);
			free((*devList)[i].minMaxClock);
		}
		if ((*devList)[i].defaultClock != NULL) {
			for (k = 0; k < NVML_CLOCK_COUNT; k++)
				free((*devList)[i].defaultClock[k]);
			free((*devList)[i].defaultClock);
		}
		free((*devList)[i].temperatures);
		free((*devList)[i].powerlimits);
		free((*devList)[i].pcieLinkInfo);
		free((*devList)[i].nvLinkBW);
		free((*devList)[i].nvLinkCount);
		(*devList)[i].dev = NULL;
	}
	free(*devList);
	*devList = NULL;
	return 0;
}
