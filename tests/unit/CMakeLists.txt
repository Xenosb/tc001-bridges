# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit)

set(APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_include_directories(app PRIVATE ${APP_SRC})
target_sources(app PRIVATE
  src/form_test.c
  src/battery_test.c
  src/brightness_test.c
  src/graphics_test.c
  src/history_test.c
  src/time_test.c
  src/format_test.c
  src/resp_scan_test.c
  ${APP_SRC}/battery_percent.c
  ${APP_SRC}/brightness_curve.c
  ${APP_SRC}/color.c
  ${APP_SRC}/font3x5.c
  ${APP_SRC}/form.c
  ${APP_SRC}/history_core.c
  ${APP_SRC}/format.c
  ${APP_SRC}/qt_logo.c
  ${APP_SRC}/resp_scan.c
  ${APP_SRC}/scroll.c
  ${APP_SRC}/timeconv.c
  ${APP_SRC}/wmo.c
)
