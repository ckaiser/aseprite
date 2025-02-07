// Aseprite
// Copyright (C) 2025  Igara Studio S.A.
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "audio.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

using namespace app;

Audio::~Audio()
{
  if (m_initialized && isPlaying())
    stop();

  if (m_soundLoaded)
    ma_sound_uninit(&m_sound);

  ma_engine_uninit(&m_engine);
}

void Audio::initialize()
{
  const auto& result = ma_engine_init(NULL, &m_engine);
  if (result != MA_SUCCESS) {
    TRACE("Audio engine failed to initialize.\n");
    return;
  }

  m_initialized = true;
}

void Audio::load(const std::string_view file)
{
  if (!m_initialized)
    initialize();

  if (m_soundLoaded)
    unloadSound();

  const auto& result = ma_sound_init_from_file(&m_engine, file.data(), 0, NULL, NULL, &m_sound);
  if (result != MA_SUCCESS) {
    TRACE("Failed to load sound from file.\n");
    return;
  }

  m_soundLoaded = true;
}

void Audio::play(const int64_t fromMilliseconds)
{
  TRACE("Audio::play!\n");

  if (fromMilliseconds >= 0)
    seek(fromMilliseconds);

  const auto& result = ma_sound_start(&m_sound);
  if (result != MA_SUCCESS)
    TRACE("Failed to start the sound\n");
}

void Audio::seek(const uint64_t milliseconds)
{
  ma_result result;
  if (milliseconds > 0) {
    result = ma_sound_seek_to_pcm_frame(&m_sound, (milliseconds / 1000.0) * sampleRate());

    if (result != MA_SUCCESS)
      TRACE("Failed to seek to frame fromMs: %s.\n");

    TRACE("Result: %d - ms: %d - frame: %d - sampleRate: %d.\n",
          result,
          milliseconds,
          (milliseconds / 1000.0) * sampleRate(),
          sampleRate());
  }
  else {
    result = ma_sound_seek_to_pcm_frame(&m_sound, 0);
    if (result != MA_SUCCESS)
      TRACE("Failed to seek to the start of the audio.\n");

    TRACE("Seeked to the start of the audio.\n");
  }
}

void Audio::stop()
{
  if (!isPlaying()) {
    TRACE("Attempted to stop a sound that's not playing.\n");
    return;
  }

  const auto& result = ma_sound_stop(&m_sound);
  if (result != MA_SUCCESS)
    TRACE("Failed to stop the sound.\n");
}

bool Audio::isPlaying() const
{
  return ma_sound_is_playing(&m_sound) != 0u;
}

void Audio::setSpeedMultiplier(const float multiplier)
{
  ma_sound_set_pitch(&m_sound, multiplier);
}

uint32_t Audio::soundLength()
{
  if (m_soundLength > 0)
    return m_soundLength;

  float length;
  const auto& result = ma_sound_get_length_in_seconds(&m_sound, &length);
  if (result == MA_SUCCESS) {
    m_soundLength = length * 1000;
    return m_soundLength;
  }

  m_soundLength = 0;
  return 0;
}

void Audio::unloadSound()
{
  ma_sound_uninit(&m_sound);
  m_soundLoaded = false;
  m_sampleRate = 0;
  m_soundLength = 0;
}

uint32_t Audio::sampleRate()
{
  if (m_sampleRate == 0) {
    const auto& result = ma_sound_get_data_format(&m_sound, NULL, NULL, &m_sampleRate, NULL, 0);
    if (result != MA_SUCCESS)
      m_sampleRate = 0;
  }

  return m_sampleRate;
}
