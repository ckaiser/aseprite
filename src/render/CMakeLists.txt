# Aseprite Render Library
# Copyright (C) 2019-2023  Igara Studio S.A.
# Copyright (C) 2001-2018 David Capello

add_library(render-lib
  error_diffusion.cpp
  get_sprite_pixel.cpp
  gradient.cpp
  ordered_dither.cpp
  quantization.cpp
  rasterize.cpp
  render.cpp
  zoom.cpp)

find_package(OpenMP REQUIRED)
target_link_libraries(render-lib OpenMP::OpenMP_CXX)

target_link_libraries(render-lib
  doc-lib
  laf-gfx
  laf-base)
