// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef ASEPRITE_TESTS_UTILS_H_INCLUDED
#define ASEPRITE_TESTS_UTILS_H_INCLUDED

#include "base/fs.h"
#include "base/time.h"
#include <gtest/gtest.h>
#include <string>

#include "fmt/args.h"
#include <fstream>

class TestTempFile {
public:
  explicit TestTempFile(const std::string& content = "", const std::string& ext = "")
  {
    static int i = 0;
    static const std::string dir = testing::TempDir();
    filename = base::join_path(
      dir,
      fmt::format("tmp_{}_{}{}", base::current_tick(), i, ext.empty() ? "" : "." + ext));
    std::ofstream out(filename);
    out << content;
    i++;
  }

  ~TestTempFile() { base::delete_file(filename); }

  std::string filename;
};

#endif // ASEPRITE_TESTS_UTILS_H_INCLUDED
