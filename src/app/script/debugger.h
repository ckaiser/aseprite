// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_SCRIPT_DEBUGGER_H_INCLUDED
#define APP_SCRIPT_DEBUGGER_H_INCLUDED
#pragma once

#include "lua.h"
#include "ui/register_message.h"

#ifdef ENABLE_SCRIPTING
  #include <map>
  #include <vector>

struct lua_State;
struct lua_Debug;

namespace dap {
class integer;
struct Variable;
struct StackFrame;
namespace net {
class Server;
}
class ReaderWriter;
class Session;
} // namespace dap

namespace app::script {
class Engine;

extern ui::RegisterMessage kDebuggerWakeMessage;

class Debugger final {
  enum class State : uint8_t {
    Pass,
    Wait,
    NextLine,
    StepIn,
    StepOut,
  };

public:
  explicit Debugger(uint16_t port);
  ~Debugger();

  void setHookState(State hookState);
  bool isWaiting() const { return m_hookState.load() == State::Wait; }

  void onHook(lua_State* L, lua_Debug* ar);

protected:
  void onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw);
  void onClientError(const char* msg) const;
  void onSessionError(const char* msg);

private:
  void updateVariables(lua_State* L);
  void updateStackTrace(lua_State* L);
  dap::Variable makeVariable(lua_State* L, const std::string& name);

  void stop(const std::string& reason, dap::integer line);

  std::unique_ptr<dap::net::Server> m_server;
  std::unique_ptr<dap::Session> m_session;
  std::multimap<std::string, int> m_breakpoints;
  std::shared_ptr<Engine> m_engine;
  std::atomic<State> m_hookState = State::Pass;

  // The currently executing stack level of the Lua script
  uint32_t m_currentStackLevel = 0;
  // The stack level actively being debugged/stepped through.
  uint32_t m_activeStackLevel = 1;

  std::vector<dap::StackFrame> m_currentStackTrace;
  std::vector<dap::Variable> m_localVariables;
  std::vector<dap::Variable> m_globalVariables;
  std::vector<std::vector<dap::Variable>> m_tableVariables;
  ;
};

} // namespace app::script

#endif // ENABLE_SCRIPTING

#endif
