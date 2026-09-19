// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context.h"
#include "app/modules/gui.h"
#include "app/ui/main_window.h"
#include "fmt/format.h"
#include "os/window.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/system.h"
#include "ver/info.h"

#include "app/i18n/strings.h"
#include "feedback.xml.h"

#if ENABLE_SENTRY
  #include "app/sentry_wrapper.h"
#endif

#include <string_view>

namespace app {

using namespace ui;
using namespace std::string_view_literals;

namespace {
constexpr int feedback_type_to_face(const std::string_view type)
{
  if (type == "positive"sv)
    return 0;
  if (type == "negative"sv)
    return 2;

  return 1;
}

std::string face_to_feedback_type(const int face)
{
  switch (face) {
    case 0:  return "positive";
    case 2:  return "negative";
    default: return "neutral";
  }
}

class FeedbackWindow final : public gen::Feedback {
public:
  explicit FeedbackWindow(const Sentry::Feedback& feedback)
  {
    faces()->ItemChange.connect(&FeedbackWindow::onFaceChange, this);

    email()->Change.connect([this] {
      // Very basic email validation
      const auto& text = email()->text();
      const size_t atPos = email()->text().find('@');
      if (!text.empty() && (atPos == std::string::npos || atPos == text.length() - 1))
        submitButton()->setEnabled(false);
      else
        submitButton()->setEnabled(true);
    });

    submitButton()->Click.connect(&FeedbackWindow::onSubmit, this);

    if (feedback.type != face_to_feedback_type(faces()->selectedItem())) {
      faces()->setSelectedItem(feedback_type_to_face(feedback.type));
      onFaceChange();
    }
    email()->setText(feedback.email);
    comments()->setText(feedback.comments);

    centerWindow();
  }

  Sentry::Feedback feedback() const
  {
    if (m_submitted)
      return {};

    return { .email = email()->text(),
             .comments = comments()->text(),
             .type = face_to_feedback_type(faces()->selectedItem()),
             .attachExtras = attachExtras()->isSelected() };
  }

  void showLoading() const
  {
    faces()->setEnabled(false);
    email()->setEnabled(false);
    comments()->setEnabled(false);
    submitButton()->setText(Strings::feedback_submitting());
    submitButton()->setEnabled(false);
  }

  void showThanks()
  {
    m_submitted = true;
    faces()->setVisible(false);
    form()->setVisible(false);
    submitButton()->setVisible(false);
    thankYou()->setVisible(true);
    close()->requestFocus();
    remapWindow();
  }

  void showError()
  {
    // Does not reset m_submitted so the user can try again later.
    faces()->setVisible(false);
    form()->setVisible(false);
    submitButton()->setVisible(false);
    error()->setVisible(true);
    close()->requestFocus();
    remapWindow();
  }

  obs::signal<void(const Sentry::Feedback&)> Submit;

private:
  void onSubmit()
  {
    Submit(feedback());
    showLoading();
  };

  void onFaceChange(ButtonSet::Item* = nullptr)
  {
    switch (faces()->selectedItem()) {
      case 0: comments()->setPlaceholder(Strings::feedback_positive_placeholder()); break;
      case 1: comments()->setPlaceholder(Strings::feedback_neutral_placeholder()); break;
      case 2: comments()->setPlaceholder(Strings::feedback_negative_placeholder()); break;
    }

    if (!form()->isVisible()) {
      form()->setVisible(true);
      actionButtons()->setVisible(true);
      remapWindow();
    }

    comments()->selectAll();
    if (!comments()->text().empty())
      comments()->requestFocus();
  }

  bool m_submitted = false;
};
} // namespace

class FeedbackCommand : public Command {
public:
  FeedbackCommand();

protected:
  void onExecute(Context* context) override;
  bool onEnabled(Context* context) override;

private:
  Sentry::Feedback m_lastFeedback;
  bool m_feedbackInProgress = false;
  bool m_showResult = false;
};

FeedbackCommand::FeedbackCommand() : Command(CommandId::Feedback())
{
}

bool FeedbackCommand::onEnabled(Context* context)
{
#if ENABLE_SENTRY
  return context->isUIAvailable() && Sentry::isInitialized() && !m_feedbackInProgress;
#endif

  return false;
}

void FeedbackCommand::onExecute(Context* context)
{
#ifndef ENABLE_SENTRY
  return;
#else
  ASSERT(m_feedbackInProgress == false);
  if (m_feedbackInProgress)
    return;

  FeedbackWindow window(m_lastFeedback);
  m_showResult = true;
  window.Submit.connect([this, &window](const Sentry::Feedback& feedback) {
    std::string type;
    m_feedbackInProgress = true;
    Sentry::sendFeedbackAsync(feedback, [this, &window](const bool result) {
      m_feedbackInProgress = false;
      if (!m_showResult)
        return;
      if (result) {
        window.showThanks();
      }
      else {
        window.showError();
      }
    });
  });

  window.openWindowInForeground();
  m_showResult = false;
  m_lastFeedback = window.feedback();
#endif
}

Command* CommandFactory::createFeedbackCommand()
{
  return new FeedbackCommand;
}

} // namespace app
