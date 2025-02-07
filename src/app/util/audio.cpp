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

#include <vector>

using namespace app;

Audio::~Audio()
{
  if (m_initialized && isPlaying())
    stop();

  if (m_loaded)
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

  if (m_loaded)
    unload();

  // TODO: Async loading
  const auto& result = ma_sound_init_from_file(
    &m_engine,
    file.data(),
    MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
    NULL,
    NULL,
    &m_sound);
  if (result != MA_SUCCESS) {
    TRACE("Failed to load sound from file.\n");
    return;
  }

  m_loaded = true;
  m_file = file;
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
  const ma_uint64 frameIndex = ((milliseconds + m_offset) / 1000.0) * sampleRate();
  const ma_result result = ma_sound_seek_to_pcm_frame(&m_sound, frameIndex);

  if (result != MA_SUCCESS)
    TRACE("Failed to seek to frame fromMs: %s.\n");

  TRACE("Result: %d - ms: %d - frame: %d - sampleRate: %d.\n",
        result,
        milliseconds,
        frameIndex,
        sampleRate());
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

std::vector<float> Audio::readWaveform()
{
  if (!m_loaded)
    return {};

  ma_format format;
  uint32_t channel_count;
  uint32_t sample_rate;

  ma_sound_get_data_format(&m_sound, &format, &channel_count, &sample_rate, nullptr, 0);

  constexpr ma_uint64 FRAMES_PER_CHUNK = 200000;
  std::vector<float> data;
  data.resize(FRAMES_PER_CHUNK * channel_count);

  ma_data_source* dataSource = ma_sound_get_data_source(&m_sound);
  int frame_count = 0;
  uint64_t frames_read;
  while (ma_data_source_read_pcm_frames(dataSource, data.data(), FRAMES_PER_CHUNK, &frames_read) !=
         MA_AT_END) {
    frame_count += frames_read;
  }

  ma_data_source_uninit(dataSource);
  return data;
}

uint32_t Audio::length()
{
  if (m_length > 0)
    return m_length;

  float length;
  const auto& result = ma_sound_get_length_in_seconds(&m_sound, &length);
  if (result == MA_SUCCESS) {
    m_length = length * 1000;
    return m_length;
  }

  m_length = 0;
  return 0;
}
void Audio::setOffset(int32_t offset)
{
  m_offset = offset;
  // TOOD: Set playback offset here or on play?
}

void Audio::unload()
{
  ma_sound_uninit(&m_sound);
  m_loaded = false;
  m_sampleRate = 0;
  m_length = 0;
  m_file = "";
  m_offset = 0;
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
