#
# Copyright (c) 2026 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# On nRF54L15 the SoC-default MCUboot partition (64 KiB) exceeds the FPROTECT
# single-region limit of 62 KiB, so MCUboot fails to build with static
# (devicetree) partitions ("Can not FPROTECT region that big"). The application
# image picks up boards/nrf54l15dk_nrf54l15_cpuapp.overlay automatically; add
# the same overlay to the MCUboot image so both agree on the shrunk partition.
#
# This is done additively (rather than via a sysbuild/mcuboot/ config dir) so
# MCUboot keeps its upstream prj.conf and app.overlay - in particular the
# "zephyr,code-partition = &boot_partition" chosen, without which MCUboot would
# relink to run from slot0.
if(SB_CONFIG_SOC_NRF54L15_CPUAPP AND SB_CONFIG_BOOTLOADER_MCUBOOT)
  add_overlay_dts(mcuboot
    ${CMAKE_CURRENT_LIST_DIR}/boards/nrf54l15dk_nrf54l15_cpuapp.overlay)
endif()
