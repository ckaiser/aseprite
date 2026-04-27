// Aseprite
// Copyright (C) 2021-2022  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#ifndef ENABLE_SCRIPTING
  #error ENABLE_SCRIPTING must be defined
#endif

#include "app/commands/debugger.h"
#include "app/context.h"
#include "app/script/debugger.h"

#include "debugger.xml.h"

namespace app {

using namespace ui;

namespace {
script::Debugger* g_debugger = nullptr;
}

class DebuggerWindow final : public gen::Debugger {
public:
  DebuggerWindow()
  {
    enableDebugger()->Click.connect([this] {
      enableDebugger()->setEnabled(false);
      disableDebugger()->setEnabled(true);
      uint16_t port = 58000;
      const auto entryPort = portEntry()->textInt();
      if (entryPort > 0 && entryPort < UINT16_MAX)
        port = portEntry()->textInt();

      ASSERT(g_debugger == nullptr);
      g_debugger = new script::Debugger(port);
    });
    disableDebugger()->Click.connect([this] {
      enableDebugger()->setEnabled(true);
      disableDebugger()->setEnabled(false);
      delete g_debugger;
      g_debugger = nullptr;
    });
  }
};

namespace {
DebuggerWindow* g_window = nullptr;
}

DebuggerCommand::DebuggerCommand() : Command(CommandId::Debugger())
{
}

bool DebuggerCommand::onEnabled(Context* context)
{
  return context->isUIAvailable();
}

void DebuggerCommand::onExecute(Context*)
{
  if (!g_window)
    g_window = new DebuggerWindow();

  g_window->openWindow();
  g_window->remapWindow();
}

Command* CommandFactory::createDebuggerCommand()
{
  return new DebuggerCommand;
}

} // namespace app
