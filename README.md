# nvmex

nvmex is a *m*etrics *ex*porter for *nv*idia graphic processing units (GPUs).
To monitor the health and resource consumption of GPUs it utilizes the nvidia
management library (libnividia-ml.so.1) usually distributed with the nvidia
driver or a similar package (e.g. Solaris: driver/graphics/nvidia,
Ubuntu: libnvidia-compute-\*). Collected data can be exposed via HTTP 
in [Prometheuse exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/)
using the endpoint URL http://_hostname:9400_/metrics (port and IP are
customizable of course) and thus visualized e.g. using [Grafana](https://grafana.com/), [Netdata](https://www.netdata.cloud/), or [Zabbix](https://www.zabbix.com/).

In contrast to Nvidia's dcgm-exporter *nvmex* is written in plain C and thus
it is compared to dcgm-exporter extremely lightweight (virtual memory size:
120 MiB vs. 5.7 GiB, resident set size: 6.5 MiB vs. 23.5..246 MiB),
does not trash your disks with error logs, or hogs any cpu.
You may run it on bare metal, or in any zone, container, or pod.


## Requirements

Beside Nvidia's libnividia-ml.so.1 (usually provided by the libnvidia-compute-XYZ package) *nvmex* requires [libprom](https://github.com/jelmd/libprom) and [libmicrohttpd](https://github.com/Karlson2k/libmicrohttpd).

The **nvml.h** (usually provided by the cuda-nvml-dev-U-V package) used to compile this utility must match the **libnividia-ml.so.1** library used on your machines and this in turn the version of the nvdia kernel module in use. The management library is usually backward compatible, so compiling against an older version and using it on machines with more recent versions of the NVML should work (but you may miss some metrics).


## Build

Adjust the **Makefile** as needed, optionally set related environment variables
(e.g. `export CUDA_VERS=10.1 CC=gcc`) and run **make**.


## Repo

The official repository for *nvmex* is https://github.com/jelmd/nvmex .
If you need some new features (or bug fixes), please feel free to create an
issue there using https://github.com/jelmd/nvmex/issues .


## Versioning

*nvmex* follows the basic idea of semantic versioning, but having the real world
in mind. Therefore official releases have always THREE numbers (A.B.C), not
more and not less! For nightly, alpha, beta, RC builds, etc. a *.0* and
possibly more dot separated digits will be append, so that one is always able
to overwrite this one by using a 4th digit > 0.


## License

[CDDL 1.1](https://spdx.org/licenses/CDDL-1.1.html)
