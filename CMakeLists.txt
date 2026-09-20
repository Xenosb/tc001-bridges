# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28.0)

# Optional local overrides (Kconfig fragment), not committed
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/local.conf)
  list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/local.conf)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tc001_bridges)

# The HTTP server keeps each service's resources in a section of its own
zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(
  NAME http_resource_desc_portal_service
  KVMA RAM_REGION GROUP RODATA_REGION
)

target_sources(app PRIVATE
  src/app_battery.c
  src/app_bridges.c
  src/app_clock.c
  src/app_screensaver.c
  src/app_settings.c
  src/app_trends.c
  src/battery.c
  src/brightness.c
  src/brightness_curve.c
  src/battery_percent.c
  src/clock.c
  src/color.c
  src/config.c
  src/font3x5.c
  src/form.c
  src/format.c
  src/gfx.c
  src/history.c
  src/history_core.c
  src/https.c
  src/icons.c
  src/light.c
  src/location.c
  src/logos.c
  src/main.c
  src/portal.c
  src/qt_logo.c
  src/resp_scan.c
  src/scroll.c
  src/setup.c
  src/stats.c
  src/timeconv.c
  src/ui.c
  src/weather.c
  src/wifi.c
  src/wmo.c
)

# Root certificates the HTTPS client trusts, embedded as C arrays
foreach(cert usertrust_ecc_root globalsign_r3_root digicert_global_g2_root gts_root_r4 isrg_root_x1)
  generate_inc_file_for_target(app
    certs/${cert}.der
    ${ZEPHYR_BINARY_DIR}/include/generated/${cert}.der.inc
  )
endforeach()
