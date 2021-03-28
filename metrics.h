#ifndef METRICS_H
#define METRICS_H

#define addPromInfo(metric) \
	psb_add_str(sb, "# HELP " metric ## _N " " metric ## _D );\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# TYPE " metric ## _N " " metric ## _T);\
	psb_add_char(sb, '\n');\

#define NVMEX_CLOCK_MHZ_D "GPU clock speeds in MHz."
#define NVMEX_CLOCK_MHZ_T "gauge"
#define NVMEX_CLOCK_MHZ_N "nvmex_clock_mhz"

#endif
