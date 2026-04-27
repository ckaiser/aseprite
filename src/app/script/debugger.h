// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_SCRIPT_DEBUGGER_H_INCLUDED
#define APP_SCRIPT_DEBUGGER_H_INCLUDED
#pragma once

#ifdef ENABLE_SCRIPTING
  #include "base/time.h"
  #include "obs/signal.h"

  #include "lua.h"

  #include "dap/protocol.h"
  #include "dap/session.h"

  #include <map>
  #include <vector>

struct lua_State;
struct lua_Debug;
struct AseLaunchRequest;
struct AseAttachRequest;

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

namespace app {
class Extension;
}

namespace app::script {
class Engine;

class Debugger final {
  enum class State : uint8_t { Pass, Wait, NextLine, StepIn, StepOut, Abort };

  struct VariableReference {
    std::string name;
    size_t index = 0;
    int ref = -1;
  };

public:
  Debugger(const std::string& address, uint16_t port);
  ~Debugger();

  void setHookState(State hookState);
  bool isWaiting() const { return m_hookState.load() == State::Wait; }
  bool isDebugging() const { return (m_engine || m_extension); }

  void onHook(lua_State* L, lua_Debug* ar);

  obs::safe_signal<void(const std::string&)> Error;

private:
  void onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw);
  void onExtensionChange(Extension* ext);
  void updateVariables(lua_State* L);
  void updateStackTrace(lua_State* L);
  dap::Variable makeVariable(lua_State* L, const std::string& name);
  void stop(const std::string& reason, dap::integer line);

  void registerHandlers();
  dap::InitializeResponse onInitialize(const dap::InitializeRequest& request);
  dap::ThreadsResponse onThreads(const dap::ThreadsRequest& request) const;
  dap::ResponseOrError<dap::AttachResponse> onAttach(const AseAttachRequest& request);
  dap::ResponseOrError<dap::LaunchResponse> onLaunch(const AseLaunchRequest& request);
  dap::ResponseOrError<dap::SetBreakpointsResponse> onSetBreakpoints(
    const dap::SetBreakpointsRequest& request);
  dap::ResponseOrError<dap::PauseResponse> onPause(const dap::PauseRequest& request);
  dap::ResponseOrError<dap::ContinueResponse> onContinue(const dap::ContinueRequest& request);
  dap::ResponseOrError<dap::NextResponse> onNext(const dap::NextRequest& request);
  dap::ResponseOrError<dap::StepInResponse> onStepIn(const dap::StepInRequest& request);
  dap::ResponseOrError<dap::StepOutResponse> onStepOut(const dap::StepOutRequest& request);
  dap::ResponseOrError<dap::StackTraceResponse> onStackTrace(const dap::StackTraceRequest& request);
  dap::ResponseOrError<dap::ScopesResponse> onScopes(const dap::ScopesRequest& request);
  dap::ResponseOrError<dap::VariablesResponse> onVariables(const dap::VariablesRequest& request);

  void onSessionError(const char* msg);
  void onSessionClosed();

  int m_threadId;
  obs::scoped_connection m_changeConn;

  std::unique_ptr<dap::net::Server> m_server;
  std::unique_ptr<dap::Session> m_session;

  std::mutex m_mutex;
  std::multimap<std::string, int> m_breakpoints;
  std::unique_ptr<Engine> m_engine;
  Extension* m_extension;
  std::atomic<State> m_hookState;
  base::tick_t m_lastHookTick;

  // The currently executing stack level of the Lua script
  uint32_t m_hookStackLevel;
  // The stack level that we want to step through.
  uint32_t m_commandStackLevel;

  std::vector<dap::StackFrame> m_currentStackTrace;
  std::vector<dap::Variable> m_locals;
  std::vector<dap::Variable> m_globals;

  std::vector<std::vector<VariableReference>> m_registry;
};

} // namespace app::script

#endif // ENABLE_SCRIPTING

#endif
