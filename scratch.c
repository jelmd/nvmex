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

/**
	gcc -m64 -I /usr/local/cuda-10.1/targets/x86_64-linux/include scratch.c \
		-L /usr/local/cuda-10.1/targets/x86_64-linux/lib/stubs -lnvidia-ml
 */
#include <stdio.h>

#include <nvml.h>

#define nverror(x)  nvmlErrorString(x)  
#ifndef uint
#define uint unsigned int
#endif

int
main(int argc, char *argv[]) {
	nvmlReturn_t res;
	uint devs, i, k, l, clockMHz;
	nvmlDevice_t dev;
	nvmlFieldValue_t fval;

	res = nvmlInit_v2();
	if (res != NVML_SUCCESS) {
		fprintf(stderr, "Failed to initialize NVML: %s\n", nverror(res));
		return 1;
	}
	res = nvmlDeviceGetCount_v2(&devs);
	if (NVML_SUCCESS != res) {
		fprintf(stderr, "Failed to get device count: %s\n", nverror(res));
		return 2;
	}
	for (i = 0; i < devs; i++) {
		res = nvmlDeviceGetHandleByIndex_v2(i, &dev);
		if (NVML_SUCCESS != res) {
			fprintf(stderr,"Failed to get device handle %u: %s\n",i,nverror(res));
			continue;
		}
		fprintf(stdout, "Device %u  (%p)\n", i, dev);
		for (l = 0; l < 20; l++) {
		for (k = 0; k < NVML_CLOCK_COUNT; k++) {
			res = nvmlDeviceGetClockInfo(dev, k, &clockMHz);
			if (NVML_SUCCESS != res) {
				fprintf(stderr,"Failed to get clock %u of %u: %s\n",k,i,nverror(res));
			}
			fprintf(stdout, "Clock %u of %u: %u MHz\n", k, i, clockMHz);
		}
		}
		fval.fieldId = NVML_FI_DEV_MEMORY_TEMP;
		//fval.scopeId = 0;
		res = nvmlDeviceGetFieldValues(dev, 1, &fval);
		if (NVML_SUCCESS == res) {
			fprintf(stdout, "NVML_FI_DEV_MEMORY_TEMP: %u %u\n", fval.valueType, fval.value.uiVal);
		}
	}
	res = nvmlShutdown();
	if (res != NVML_SUCCESS) {
		fprintf(stderr, "Failed to shutdown NVML: %s\n", nverror(res));
		return 3;
	}
	return 0;
}
