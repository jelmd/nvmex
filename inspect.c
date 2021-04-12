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

uint
listDevices(psb_t *sb) {
	nvmlReturn_t res;
	uint devs, i;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;

	// Does NOT count devices NVML has no permission to talk to.
	res = nvmlDeviceGetCount(&devs);
	if (NVML_SUCCESS != res) { 
		PROM_WARN("Failed to get device count: %s", nverror(res));
		return 0;
	}
    PROM_DEBUG("Found %u device%s", devs, devs != 1 ? "s" : "");
	if (devs == 0)
		return 0;

	if (free_sb)
		sb = psb_new();

	psb_add_str(sb, "Devices found:\n");    
	for (i = 0; i < devs; i++) {
		nvmlDevice_t dev;
		char name[NVML_DEVICE_NAME_BUFFER_SIZE];
		nvmlPciInfo_t pci;
		nvmlComputeMode_t compute_mode;

		res = nvmlDeviceGetHandleByIndex(i, &dev);
		if (NVML_SUCCESS != res) { 
			PROM_WARN("Failed to get device handle %u: %s\n", i, nverror(res));
			continue;
		}
		// see also  nvmlDeviceGetArchitecture(dev, &arch): maps arch to string
		res = nvmlDeviceGetName(dev, name, NVML_DEVICE_NAME_BUFFER_SIZE);
		if (NVML_SUCCESS != res) { 
            printf("Failed to get name of device %u: %s\n", i,
				nverror(res));
            continue;
        }
		res = nvmlDeviceGetPciInfo(dev, &pci);
		if (NVML_SUCCESS != res) { 
			PROM_WARN("Failed to get pci info for device %u: %s\n", i,
				nverror(res));
			continue;
		}
		res = nvmlDeviceGetComputeMode(dev, &compute_mode);
		snprintf(buf, MBUF_SZ, "%u. %s [%s]   %sCUDA capable\n",
			i, name, pci.busId, NVML_ERROR_NOT_SUPPORTED == res ? "not " : "");
		psb_add_str(sb, buf);
	}
	if (free_sb) {
		fprintf(stderr, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return devs;
}

// System Queries 2.12
uint
getVersions(psb_t *sb) {
	bool free_sb = sb == NULL;
	nvmlReturn_t res;
	int v;
	char buf[MBUF_SZ];

	if (free_sb)
		sb = psb_new();

	res = nvmlSystemGetCudaDriverVersion(&v);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get CUDA driver version: %s\n", nverror(res));
	} else {
		int min = v%1000;
		snprintf(buf, MBUF_SZ, "CUDA driver version: %d.%d.%d\n",
			v/1000, min/10, min%10);
		psb_add_str(sb, buf);
	}

	res = nvmlSystemGetDriverVersion (buf, MBUF_SZ);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get System driver version: %s\n", nverror(res));
	} else {
		psb_add_str(sb, "Kernel module version: ");
		psb_add_str(sb, buf);
		psb_add_char(sb, '\n');
	}

	res = nvmlSystemGetNVMLVersion (buf, MBUF_SZ);
	if (NVML_SUCCESS != res) {
		PROM_WARN("Failed to get NVML version: %s\n", nverror(res));
	} else {
		psb_add_str(sb, "NVML version: ");
		psb_add_str(sb, buf);
		psb_add_char(sb, '\n');
	}
	//res = nvmlSystemGetProcessName (pid, buf, MBUF_SZ) 	// maps pid to name

	if (free_sb) {
		fprintf(stderr, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return 0;
}

// Unit Queries 2.13
uint
getDevInfos(psb_t *sb) {
	bool free_sb = sb == NULL;
	nvmlReturn_t res;
	//char buf[MBUF_SZ];
	uint units;

	res = nvmlUnitGetCount(&units);
	if (NVML_SUCCESS != res) { 
		PROM_WARN("Failed to get unit count: %s", nverror(res));
		return 0;
	}
	PROM_DEBUG("Found %d units.", units);
	if (units == 0)
		return 0;

	/* Skip, 'til nvidia comes up with an explaination, what "units" and
	   "S-class {systems|products}" are.
	*/
	if (free_sb)
		sb = psb_new();

	if (free_sb) {
		fprintf(stderr, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
	return 0;
}

// Device Queries 2.14
uint
getDevices(psb_t *sb, gpu_t *gpuList[]) {
	nvmlReturn_t res;
	uint devs, i, count = 0, k;
	nvmlDevice_t *devList;
	nvmlPciInfo_t pci;
	char buf[MBUF_SZ];
	bool free_sb = sb == NULL;
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

	if (free_sb)
		sb = psb_new();
	k = 0;
	for (i = 0; i < devs; i++) {
		if (devList[i] == NULL)
			continue;
		PROM_DEBUG("gpu[%u] = %p", i, &((*gpuList)[i]));
		(*gpuList)[k].dev = devList[i];
		(*gpuList)[k].idx = i;
		res = nvmlDeviceGetPciInfo((*gpuList)[k].dev, &pci);
		if (NVML_SUCCESS != res) {
			PROM_WARN("Failed to get pci info dev %u: %s", i, nverror(res));
		} else {
			(*gpuList)[k].pciId = strdup(pci.busId);
		}
		res = nvmlDeviceGetUUID((*gpuList)[k].dev, buf, MBUF_SZ);
		if (NVML_SUCCESS != res) {
			PROM_WARN("Failed to get UUID dev %u: %s", i, nverror(res));
		} else {
			(*gpuList)[k].uuid = strdup(buf + 4);	// now it is a propper UUID
		}
		snprintf(buf, MBUF_SZ, "# gpu[%u]: %s  %s  %p\n", (*gpuList)[k].idx,
			(*gpuList)[k].uuid, (*gpuList)[k].pciId, (void *)((*gpuList)[k]).dev);
		psb_add_str(sb, buf);
		k++;
	}

end:
	free(devList);
	devList = NULL;
	if (free_sb) {
		fprintf(stderr, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}

	return devs;
}

uint
cleanup(uint devs, gpu_t *devList[]) {
	uint i, k;

	if (*devList == NULL || devs == 0)
		return 0;

	for (i = 0; i < devs; i++) {
		free((*devList)[i].uuid);
		free((*devList)[i].pciId);
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
		(*devList)[i].dev = NULL;
	}
	free(*devList);
	*devList = NULL;
	return 0;
}
