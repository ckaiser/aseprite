// Aseprite
// Copyright (C) 2025  Igara Studio S.A.
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UTIL_AUDIO_H_INCLUDED
#define APP_UTIL_AUDIO_H_INCLUDED
#pragma once

#define WIN32_LEAN_AND_MEAN
#include "miniaudio.h"

#include <string_view>

namespace app {
class Audio {
public:
  explicit Audio() = default;
  ~Audio();

  void initialize();
  void load(std::string_view file);
  void play(int64_t fromMilliseconds = -1);
  void stop();
  void seek(uint64_t milliseconds);
  bool isPlaying() const;
  uint32_t soundLength();
  void setSpeedMultiplier(float multiplier);

protected:
  void unloadSound();
  uint32_t sampleRate();

private:
  bool m_initialized = false;
  bool m_soundLoaded = false;

  // Cached to avoid calls
  uint32_t m_sampleRate = 0;
  uint32_t m_soundLength = 0;

  ma_engine m_engine;
  ma_sound m_sound;
};
} // namespace app

#endif
