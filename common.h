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

#ifndef NVMEX_COMMON_H
#define NVMEX_COMMON_H

#define nverror(x)  nvmlErrorString(x)  
#ifndef uint
#define uint unsigned int
#endif

#include <stdbool.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <nvml.h>
#pragma GCC diagnostic pop

#include <prom_string_builder.h>
#include <prom_log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GPU data, we need to collect once, only.
 */
typedef struct gpu_st {
	nvmlDevice_t	dev;	//!< device handle for the GPU
	uint	idx;			//!< NVML index of the GPU. May change on reboot.
	char	*uuid;			//!< UUID of the GPU - survives reboot.
	char	*pciId;	//!< PCI BUS ID - survives reboot if slot does not change.
	char	**minMaxClock;	//<! PROM metrics for minMax Clocks 
	char	**defaultClock;	//<! PROM metrics for default Clock
} gpu_t;


#define addPromInfo(metric) \
	psb_add_str(sb, "# HELP " metric ## _N " " metric ## _D );\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# TYPE " metric ## _N " " metric ## _T);\
	psb_add_char(sb, '\n');\

#define NVMEX_CLOCK_MHZ_D "GPU clock speeds in MHz."
#define NVMEX_CLOCK_MHZ_T "gauge"
#define NVMEX_CLOCK_MHZ_N "nvmex_clock_mhz"

#ifdef __cplusplus
}
#endif

#endif // NVMEX_COMMON_H
