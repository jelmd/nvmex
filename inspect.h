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
 * @file inspect.h
 * A collection of small utilities to get some metrics about GPUs currently
 * available on the machine running it.
 */

#ifndef NVMEX_INSPECT_H
#define NVMEX_INSPECT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the NVML (NVidia Management Library).
 * @return \c 0 on success, a number > 0 otherwise.
 */
uint start(void);

/**
 * @brief Shutdown the NVML.
 * @return \c 0 on success, a number > 0 otherwise.
 */
uint stop(void);


char *getVersions(psb_t *report, bool compact);

/**
 * @brief List devices.
 * @param report	Where to append prom formatted result. Migh be \c NULL .
 * @param compact	If \c true omit any comments incl. metrics desvcription.
 * @param devs	The lengt of the array passed as \c devList .
 * @return a human readable report.
 */
char *getDevInfos(psb_t *report, bool compact, uint devs, gpu_t devList[]);

/**
 * Get a list of all accessible devices. If a device is not accessible, the
 * corresponding entry in the list is \c NULL. Entries in the list have the
 * same index as the NVML device index (and usually ordered by pci address).
 *
 * @param devList	where to store the pointer to the generated device list.
 * @return the number of available device, the size of \c devList .
 */
uint getDevices(gpu_t *devList[]);

uint getUnitInfos(psb_t *sb);

/**
 * Cleanup the list of gpu devices obtained via \c getDevices() to avoid
 * memory leaks.
 * @param devs	number of devices in \c devList.
 * @param devList	the address of the pointer to the device list to cleanup.
 * @return the number of remaining device in the list (size of \c devList ).
 */
uint cleanup(uint devs, gpu_t *devList[]);

#ifdef __cplusplus
}
#endif

#endif	// NVMEX_INSPECT_H
