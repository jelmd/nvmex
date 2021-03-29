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
 * @file bar1memory.h
 * A collection of small utilities to get some metrics about GPUs currently
 * available on the machine running it.
 */

#ifndef NVMEX_BAR1MEMORY_H
#define NVMEX_BAR1MEMORY_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get BAR1 memory metrics.
 * BAR1 is used to map the FB (device memory) so that it can be directly
 * accessed by the CPU or by 3rd party devices (peer-to-peer on the PCIE bus).
 * Kepler+.
 *
 * @param sb	where to append the metrics.
 * @param compact	If \c true do not add prom descriptions and type comments.
 * @param devs	number of devices in \c devList.
 * @param devList	list of devices to query. Must not be \c NULL !
 * @return \c true if something got append to \c sb , \c false otherwise.
 */
bool getBar1memory(psb_t *sb, bool compact, uint devs, gpu_t devList[]);

#ifdef __cplusplus
}
#endif

#endif	// NVMEX_BAR1MEMORY_H
