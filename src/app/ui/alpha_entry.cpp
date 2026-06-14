// Aseprite UI Library
// Copyright (C) 2024  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "app/ui/alpha_entry.h"
#include "gfx/rect.h"
#include "obs/signal.h"
#include "pref.xml.h"
#include "ui/slider.h"

namespace app {

using namespace gfx;

AlphaEntry::AlphaEntry(AlphaSlider::Type type) : IntEntry(0, 255)
{
  m_slider = std::make_unique<AlphaSlider>(0, type);
  m_slider->setFocusStop(false); // In this way the IntEntry doesn't lost the focus
  m_slider->setTransparent(true);
  m_slider->Change.connect([this] { this->onChangeSlider(); });
}

int AlphaEntry::getValue() const
{
  int value = m_slider->convertTextToValue(text());
  if (static_cast<AlphaSlider*>(m_slider.get())->getAlphaRange() ==
      app::gen::AlphaRange::PERCENTAGE)
    value = std::round(((double)m_slider->getMaxValue()) * ((double)value) / ((double)100));

  return std::clamp(value, m_min, m_max);
}

void AlphaEntry::setValue(int value)
{
  value = std::clamp(value, m_min, m_max);

  if (m_popupWindow && !m_changeFromSlider)
    m_slider->setValue(value);

  if (static_cast<AlphaSlider*>(m_slider.get())->getAlphaRange() ==
      app::gen::AlphaRange::PERCENTAGE)
    value = std::round(((double)100) * ((double)value) / ((double)m_slider->getMaxValue()));

  setText(m_slider->convertValueToText(value));

  onValueChange();
}

} // namespace app
