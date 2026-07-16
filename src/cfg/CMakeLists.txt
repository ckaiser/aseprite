# Aseprite Config Library
# Copyright (c) 2026-present Igara Studio S.A.
# Copyright (c) 2014-2016 David Capello

add_library(cfg-lib cfg.cpp)

target_link_libraries(cfg-lib
  laf-base
  SimpleIni)
