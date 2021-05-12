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

#define NVMEX_VERSION "1.0.0"
#define NVMEX_AUTHOR "Jens Elkner (jel+nvmex@cs.uni-magdeburg.de)"

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
	char	*uuid;			//!< UUID of the GPU - survives reboot.
	char	*pciId;	//!< PCI BUS ID - survives reboot if slot does not change.
	char	*info;
	char	**minMaxClock;	//<! PROM metrics for minMax Clocks 
	char	**defaultClock;	//<! PROM metrics for default Clock
	char	*temperatures;	//<! static temperature values
	char	*powerlimits;	//<! static power limits
	char	*pcieLinkInfo;	//<! static PCIe infos
	char	*nvLinkBW;		//<! static NvLinkBandwith
	char	*nvLinkCount;	//<! static number of NvLinks
	uint	idx;			//!< NVML index of the GPU. May change on reboot.
	int		hasViolation;	//!< Bitmask about supported violation durations
	char	hasClockThrottle;
	char	hasBar1memory;
	char	hasTemperature;
	char	hasTemperatureMem;
	char	hasPowerConsum;
	char	hasPowerLimit;
	char	hasPower;
	char	hasPstate;
	char	hasFan;
	char	hasUtil;
	char	hasDecoderUtil;
	char	hasPCIeUtil;
	char	hasPCIeReplay;
	char	hasECC;
	char	hasRetiredPages;
	char	hasRemappedRows;
	char	hasNvLinks;
	char	nvLinkFieldError;
	char	nvLinks;		// sum up this number of nvLinks wrt. tx & rx
#ifndef NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX
	char	nvLinkSkipTxRx[NVML_NVLINK_MAX_LINKS + 1];
	char	nvLinkTxRxError;
#endif
	char	hasEncStats;
	char	hasEncSessions;
	char	hasFbcStats;
	char	hasFbcSessions;
} gpu_t;

#define MBUF_SZ 256

#define addPromInfo(metric) {\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# HELP " metric ## _N " " metric ## _D );\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# TYPE " metric ## _N " " metric ## _T);\
	psb_add_char(sb, '\n');\
}

#define NOT_AVAIL(x) \
	((x) == NVML_ERROR_NOT_SUPPORTED || (x) == NVML_ERROR_GPU_IS_LOST)

#define NVMEXM_VERS_D "Software version information."
#define NVMEXM_VERS_T "gauge"
#define NVMEXM_VERS_N "nvmex_version"

#define NVMEXM_GPU_D "Misc. GPU information."
#define NVMEXM_GPU_T "gauge"
#define NVMEXM_GPU_N "nvmex_gpu_info"

#define NVMEXM_CLOCK_D "GPU clock speeds in MHz."
#define NVMEXM_CLOCK_T "gauge"
#define NVMEXM_CLOCK_N "nvmex_clock_MHz"

#define NVMEXM_CLOCK_THROTTLE_D "Current reasons for GPU clock throttling."
#define NVMEXM_CLOCK_THROTTLE_T "gauge"
#define NVMEXM_CLOCK_THROTTLE_N "nvmex_clock_throttle_bitmask"

#define NVMEXM_BAR1MEM_D "BAR1 memory in bytes."
#define NVMEXM_BAR1MEM_T "gauge"
#define NVMEXM_BAR1MEM_N "nvmex_bar1mem_B"

#define NVMEXM_TEMPERATURE_D "Device temperatures and thresholds in degrees celsius."
#define NVMEXM_TEMPERATURE_T "gauge"
#define NVMEXM_TEMPERATURE_N "nvmex_temperature_C"

#define NVMEXM_POWER_CONSUM_D "Power consumption since last driver [re]load in millijoule."
#define NVMEXM_POWER_CONSUM_T "counter"
#define NVMEXM_POWER_CONSUM_N "nvmex_power_consumption_mJ"

#define NVMEXM_POWER_D "Power usage and limits in milliwatt."
#define NVMEXM_POWER_T "gauge"
#define NVMEXM_POWER_N "nvmex_power_mW"

#define NVMEXM_PSTATE_D "Current performance state (0=max .. 15=min)."
#define NVMEXM_PSTATE_T "gauge"
#define NVMEXM_PSTATE_N "nvmex_perf_state"

#define NVMEXM_FAN_D "Fan speed settings in percent."
#define NVMEXM_FAN_T "gauge"
#define NVMEXM_FAN_N "nvmex_fan_speed_pct"

#define NVMEXM_UTIL_D "GPU device utilizations in percent."
#define NVMEXM_UTIL_T "gauge"
#define NVMEXM_UTIL_N "nvmex_util_pct"

#define NVMEXM_PCIE_UTIL_D "PCIe utilization in bytes/s measured for 20 ms."
#define NVMEXM_PCIE_UTIL_T "gauge"
#define NVMEXM_PCIE_UTIL_N "nvmex_pcie_util_Bps"

#define NVMEXM_PCIE_REPLAY_D "PCIe replay count."
#define NVMEXM_PCIE_REPLAY_T "counter"
#define NVMEXM_PCIE_REPLAY_N "nvmex_pcie_replay_total"

#define NVMEXM_PCIE_LINK_D "PCIe link information."
#define NVMEXM_PCIE_LINK_T "counter"
#define NVMEXM_PCIE_LINK_N "nvmex_pcie_link"

#define NVMEXM_VIOL_D "How long a policy caused the GPU to be below application or base clocks."
#define NVMEXM_VIOL_T "counter"
#define NVMEXM_VIOL_N "nvmex_violation_penalty_ms"

#define NVMEXM_MEM_D "Device memory in bytes."
#define NVMEXM_MEM_T "gauge"
#define NVMEXM_MEM_N "nvmex_memory_bytes"

#define NVMEXM_ECC_MODE_D "ECC mode of the device (0 .. disabled, 1.. enabled)."
#define NVMEXM_ECC_MODE_T "gauge"
#define NVMEXM_ECC_MODE_N "nvmex_ecc_mode"

#define NVMEXM_ECC_ERR_D "ECC single and double bit errors (SBE and DBE) grouped by persistent and volatile (i.e. since last driver load/reboot) counter as well as where it occured (Ln .. Ln cache, GPU .. device memory, REG .. register file, TEX .. texture memory, CBU .. convergence barrier unit, ALL .. aggregation of all locations mentioned before)."
#define NVMEXM_ECC_ERR_T "gauge"
#define NVMEXM_ECC_ERR_N "nvmex_ecc_errors"

#define NVMEXM_ECC_PAGE_D "Number of retired memory pages because of multiple single bit errors (sbe), or double bit errors (dbe), or whether there are any pages are pending retirement (pending), i.e. need reboot to fully retire."
#define NVMEXM_ECC_PAGE_T "counter"
#define NVMEXM_ECC_PAGE_N "nvmex_ecc_retired_pages"

#define NVMEXM_ECC_ROW_D "Number of remapped rows ('pending','failure': 0 .. no, 1 .. yes)."
#define NVMEXM_ECC_ROW_T "counter"
#define NVMEXM_ECC_ROW_N "nvmex_ecc_remapped_rows"

#define NVMEXM_NVLINK_ERR_D "NVLink related errors and retries."
#define NVMEXM_NVLINK_ERR_T "counter"
#define NVMEXM_NVLINK_ERR_N "nvmex_nvlink_errors"

#define NVMEXM_NVLINK_BW_D "Common NVLink bandwidth in MB/s for active links."
#define NVMEXM_NVLINK_BW_T "counter"
#define NVMEXM_NVLINK_BW_N "nvmex_nvlink_bandwidth_MBps"

#define NVMEXM_NVLINK_COUNT_D "Number of NVLinks present on the device."
#define NVMEXM_NVLINK_COUNT_T "counter"
#define NVMEXM_NVLINK_COUNT_N "nvmex_nvlink_links"

#define NVMEXM_NVLINK_TRAFFIC_D "NVLink traffic counter in bytes."
#define NVMEXM_NVLINK_TRAFFIC_T "counter"
#define NVMEXM_NVLINK_TRAFFIC_N "nvmex_nvlink_traffic_bytes"

#define NVMEXM_ENCSTAT_SESS_D "Number of active encoder sessions."
#define NVMEXM_ENCSTAT_SESS_T "gauge"
#define NVMEXM_ENCSTAT_SESS_N "nvmex_enc_stat_sessions"

#define NVMEXM_ENCSTAT_FPS_D "Trailing average encode frames per second of all active sessions."
#define NVMEXM_ENCSTAT_FPS_T "gauge"
#define NVMEXM_ENCSTAT_FPS_N "nvmex_enc_stat_fps"

#define NVMEXM_ENCSTAT_LAT_D "Average encode latency in microseconds."
#define NVMEXM_ENCSTAT_LAT_T "gauge"
#define NVMEXM_ENCSTAT_LAT_N "nvmex_enc_stat_latency_us"

#define NVMEXM_ENCSESS_FPS_D "Moving average encode frames per second."
#define NVMEXM_ENCSESS_FPS_T "gauge"
#define NVMEXM_ENCSESS_FPS_N "nvmex_enc_session_fps"

#define NVMEXM_ENCSESS_LAT_D "Moving average encode latency in microseconds."
#define NVMEXM_ENCSESS_LAT_T "gauge"
#define NVMEXM_ENCSESS_LAT_N "nvmex_enc_session_latency_us"

#define NVMEXM_FBCSTAT_SESS_D "Total no of frame capture sessions."
#define NVMEXM_FBCSTAT_SESS_T "gauge"
#define NVMEXM_FBCSTAT_SESS_N "nvmex_fbc_stat_sessions"

#define NVMEXM_FBCSTAT_FPS_D "Moving average new frames captured per second."
#define NVMEXM_FBCSTAT_FPS_T "gauge"
#define NVMEXM_FBCSTAT_FPS_N "nvmex_fbc_stat_fps"

#define NVMEXM_FBCSTAT_LAT_D "Moving average new frame capture latency in microseconds."
#define NVMEXM_FBCSTAT_LAT_T "gauge"
#define NVMEXM_FBCSTAT_LAT_N "nvmex_fbc_stat_latency_us"

#define NVMEXM_FBCSESS_FPS_D "Moving average new frames captured per second."
#define NVMEXM_FBCSESS_FPS_T "gauge"
#define NVMEXM_FBCSESS_FPS_N "nvmex_fbc_session_fps"

#define NVMEXM_FBCSESS_LAT_D "Moving average new frame capture latency in microseconds."
#define NVMEXM_FBCSESS_LAT_T "gauge"
#define NVMEXM_FBCSESS_LAT_N "nvmex_fbc_session_latency_us"

/*
#define NVMEXM_XXX_D "short description."
#define NVMEXM_XXX_T "gauge"
#define NVMEXM_XXX_N "nvmex_yyy"

 */

#ifdef __cplusplus
}
#endif

#endif // NVMEX_COMMON_H
