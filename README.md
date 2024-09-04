## Demystifying Datapath Accelerator Enhanced Off-path SmartNIC [ICNP24]

A framework to understand BlueField-3 Datapath Accelerator(DPA) performance. This is the source code for our ICNP24 paper.

## Required hardware and software

- BlueField-3 SmartNIC with any series, running DPU mode/ NIC mode
- NVIDIA DOCA Framework >= 2.5.0
- DPDK >= 22.11
- Required lib: cmake, gflags, numa, lz4, z
- HugePage: At least 2048 huge pages on specific NUMA node

## Required settings

All benchmarks require

- two BF3 SmartNICs connect under 400Gbps **Ethernet**(back-to-back or switch)
- use **link aggregation** to combine two network interfaces into a single interface(mlx5_bond_0), here is [a guideline](https://docs.nvidia.com/networking/display/bluefielddpuosv450/link+aggregation) for how to setup
- enable OVS hardware offload
- set PCI_WR_ORDERING to force_relax
- isolation cpu for test (add isolcpus=0-11 nohz_full=0-11 to /etc/default/grub and reboot)
- disable PFC

## DPU Mode / NIC Mode

**Due to DOCA driver limitation, DPA can only access Arm memory under DPU mode, and access host memory under NIC mode. So please change to correct mode before benchmark.** Here is a sample for change mode:

```bash
# To NIC MODE
mlxconfig -d /dev/mst/mt41692_pciconf0 s INTERNAL_CPU_OFFLOAD_ENGINE=1

# TO DPU MODE
mlxconfig -d /dev/mst/mt41692_pciconf0 s INTERNAL_CPU_OFFLOAD_ENGINE=0

# Then power cycle
```

## Benchmark description

All DPA bench codes located in /flexio folder, the benchmarks used in the paper are described below. This repository contains other benchmarks(For Arm processor or hardware engines) as well.

| Benchmark                  | Description                                                |
| -------------------------- | ---------------------------------------------------------- |
| bench/computer             | used for test DPA roofline                                 |
| bench/memory               | used for test DPA cache/memory                             |
| dpa_kv_aggregation         | used for test kv aggregation case study                    |
| dpa_network_function       | used for test NFV case study                               |
| dpa_refactor               | a sample code for L2 reflector                             |
| dpa_refactor_ddio          | used for test DPA DDIO                                     |
| dpa_refactor_mt            | multi DPA thread version for L2 reflector                  |
| dpa_refactor_random_access | used for test random access in memory when handing packets |
| dpa_refactor_workingset    | used for test working set size' influence for DPA          |
| dpa_send                   | a sample code for send packet use DPA                      |
| dpa_send_mt                | multi DPA thread version for dpa_send                      |
| dpa_send_ntp               | used for ntp time sync case study                          |
| dpa_small_bank             | DPA version smallbank test                                 |
| roofline                   | used for test DPA roofline                                 |

## Others

- /test_suite folder contains other BF3 benchmark code, e.g., dma_copy_bench used for benchmark DOCA DMA performance, and so on.
- /scripts folder contains some data process and automatic run some benchmark with differenct params.

## WIP

All DPDK codes are still working in code format process, but it's partial located in https://github.com/cxz66666/dpdk_cs/tree/dpdk22.11/benchmark , you also can try to write another version with different DPDK implement.

### Contact

email at [chenxuz@zju.edu.cn](mailto:chenxuz@zju.edu.cn)

### Cite

```
// WIP
```
