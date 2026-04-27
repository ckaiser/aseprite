// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/resource_finder.h"
#include "app/script/debugger.h"
#include "base/exception.h"
#include "base/fs.h"
#include "base/split_string.h"
#include "engine_manager.h"
#include "ui/manager.h"
#include "ui/message_loop.h"
#include "ui/system.h"

#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"
#include "fmt/format.h"
#include "luacpp.h"

// TODO: Remove
// #define TRACE_DAP(description, object) \
//  { \
//    dap::json::NlohmannSerializer s; \
//    s.serialize(object); \
//    TRACEARGS(description, s.dump()); \
//  }

// Arbitrary "main thread" ID for cppdap
#define DBG_THREAD_ID 100

struct CheckLStack { // TODO: Remove me
  lua_State* L;
  int top;
  explicit CheckLStack(lua_State* L) : L(L), top(lua_gettop(L)) {}
  ~CheckLStack()
  {
    PRINTARGS("top", top, "lua_gettop", lua_gettop(L));
    ASSERT(top == lua_gettop(L));
  }
};

struct AseLaunchRequest : dap::LaunchRequest {
  using Response = dap::LaunchResponse;
  dap::optional<dap::string> extensionName;
  dap::optional<dap::string> scriptFilename;
};

namespace dap {
DAP_STRUCT_TYPEINFO_EXT(AseLaunchRequest,
                        LaunchRequest,
                        "launch",
                        DAP_FIELD(extensionName, "extensionName"),
                        DAP_FIELD(scriptFilename, "scriptFilename"));
} // namespace dap

namespace app::script {

ui::RegisterMessage kDebuggerWakeMessage;

Debugger::Debugger(const uint16_t port)
{
  m_server = dap::net::Server::create();
  const bool result = m_server->start(
    "0.0.0.0",
    port,
    [this](const auto& rw) { onClientConnected(rw); },
    [this](const char* msg) { onClientError(msg); });
  if (!result)
    throw base::Exception("Could not create debugger server.");

  TRACEARGS("[DBG] Initialized");
}

Debugger::~Debugger()
{
  m_session.reset();
  m_server->stop();
}

void Debugger::setHookState(const State hookState)
{
  if (m_hookState == hookState)
    return;

  m_hookState = hookState;

  if (hookState == State::Wait)
    return;

  // When we're going from a wait state to another, we need to send a message to the queue wake up
  // the loop inside the hook
  ui::execute_from_ui_thread(
    [] { ui::Manager::getDefault()->enqueueMessage(new ui::Message(kDebuggerWakeMessage)); });
}

void Debugger::updateVariables(lua_State* L)
{
  m_tableVariables.clear();
  m_localVariables.clear();

  lua_Debug ar;
  if (lua_getstack(L, 0, &ar)) {
    for (int n = 1;; ++n) {
      const char* name = lua_getlocal(L, &ar, n);
      if (!name)
        break;

      // These special names are returned by luaG_findlocal()
      if (strcmp(name, "(temporary)") == 0 || strcmp(name, "(C temporary)") == 0) {
        lua_pop(L, 1);
        continue;
      }

      m_localVariables.push_back(makeVariable(L, name));
      lua_pop(L, 1);
    }
  }

  // Updating globals
  m_globalVariables.clear();
  try {
    lua_pushglobaltable(L);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      const char* name = lua_tostring(L, -2);
      if (!name)
        break;

      m_globalVariables.push_back(makeVariable(L, name));
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  catch (std::exception& ex) {
    TRACE("OH");
  }
}

void Debugger::updateStackTrace(lua_State* L)
{
  m_currentStackTrace.clear();
  lua_Debug ar;
  int level = 0;
  while (lua_getstack(L, level++, &ar)) {
    dap::StackFrame frame{ .id = level };
    lua_getinfo(L, "Slntf", &ar);
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    const std::string filename(ar.short_src, ar.srclen - 1);

    if (!filename.empty() && filename != "[C]")
      frame.source = dap::Source{ .path = filename };

    if (ar.currentline > 0)
      frame.line = ar.currentline;

    // TODO: If we're inside a pcall we lose ar.name
    if (ar.name) {
      frame.name = ar.name;
    }
    else if (strcmp(ar.what, "main") == 0) {
      frame.name = "[main]";
    }
    else {
      frame.name = "[unnamed function]";
    }

    if (ar.istailcall)
      frame.name += " (tailcall)";
    m_currentStackTrace.push_back(frame);

    lua_pop(L, 1);
  }
}
dap::Variable Debugger::makeVariable(lua_State* L, const std::string& name)
{
  dap::Variable var{ .name = name };
  if (name == "package" || name == "_G" || name == "_ENV")
    return var;
  switch (lua_type(L, -1)) {
    case LUA_TNIL:
      var.type = "nil";
      var.value = "nil";
      break;
    case LUA_TBOOLEAN:
      var.type = "boolean";
      var.value = lua_toboolean(L, -1) ? "true" : "false";
      break;
    case LUA_TNUMBER:
      var.type = "number";
      lua_pushvalue(L, -1);
      var.value = lua_tostring(L, -1);
      lua_pop(L, 1);
      break;
    case LUA_TSTRING:
      var.type = "string";
      var.value = lua_tostring(L, -1);
      break;
    case LUA_TLIGHTUSERDATA: var.type = "lightuserdata"; break;
    case LUA_TTABLE:         {
      var.type = "table";
      std::vector<dap::Variable> variables;
      size_t count = 0;
      lua_pushnil(L);
      try {
        while (lua_next(L, -2) != 0) {
          count++;
          lua_pushvalue(L, -2);
          const std::string key = lua_tostring(L, -1);
          lua_pop(L, 1);
          variables.push_back(makeVariable(L, key));
          lua_pop(L, 1);
        }
      }
      catch (std::exception& ex) {
        var.value = "!ERROR!";
        return var;
      }
      var.namedVariables = count;
      m_tableVariables.push_back(variables);
      var.variablesReference = m_tableVariables.size();
      break;
    }
    case LUA_TFUNCTION: var.type = "function"; break;
    case LUA_TUSERDATA: var.type = "userdata"; break;
    case LUA_TTHREAD:   var.type = "thread"; break;
    default:            break;
  }
  // TRACEARGS("makeVariable{ name =", var.name, ", type =", var.type.has_value() ? var.type.value()
  // : "none", ", value = ", (var.variablesReference > 0) ? fmt::format("[ref={}]",
  // (int)var.variablesReference) : var.value, "}");
  return var;
}

// Sends the stop event and sets the hook state to "wait"
// Valid reasons:
// 'step', 'breakpoint', 'exception', 'pause', 'entry', 'goto', 'function breakpoint',
// 'data breakpoint', 'instruction breakpoint'
void Debugger::stop(const std::string& reason, const dap::integer line)
{
  TRACEARGS("Hit breakpoint at", line, reason);
  dap::StoppedEvent event{
    .reason = reason,
    .threadId = DBG_THREAD_ID,
  };
  if (line > 0)
    event.hitBreakpointIds = { line };
  m_session->send(event);
  setHookState(State::Wait);
}

void Debugger::onHook(lua_State* L, lua_Debug* ar)
{
  ASSERT(m_engine);
  ASSERT(m_session);
  ASSERT(ar);

  int ret = lua_getinfo(L, "lSn", ar);
  if (ret == 0 || ar->currentline < 0)
    return;

  const std::string filename = base::normalize_path(std::string(ar->short_src, ar->srclen - 1));

  switch (ar->event) {
    case LUA_HOOKCALL: ++m_currentStackLevel; break;
    case LUA_HOOKRET:  --m_currentStackLevel; break;
    case LUA_HOOKLINE:
      ASSERT(ar->currentline > 0);
      if (m_hookState.load() == State::NextLine && m_currentStackLevel <= m_activeStackLevel) {
        m_activeStackLevel = m_currentStackLevel;
        stop("step", ar->currentline);
        break;
      }

      for (auto [itr, rangeEnd] = m_breakpoints.equal_range(filename); itr != rangeEnd; ++itr) {
        if (itr->second == ar->currentline) {
          stop("breakpoint", ar->currentline);
          break;
        }
      }
      break;
  }

  if (m_hookState.load() == State::Wait) {
    updateVariables(L);
    updateStackTrace(L);

    ui::MessageLoop loop(ui::Manager::getDefault());
    while (m_hookState.load() == State::Wait)
      loop.pumpMessages();
  }
}

void Debugger::onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw)
{
  TRACEARGS("[DBG] Client connected");
  m_session = dap::Session::create();
  m_session->onError([this](const char* msg) { onSessionError(msg); });

  m_session->registerHandler([](const dap::InitializeRequest& request) {
    TRACEARGS("[DBG] Server received initialize request from client:", request.clientName.value());
    dap::InitializeResponse response;
    response.supportsReadMemoryRequest = false;
    response.supportsDataBreakpoints = false;
    response.supportsExceptionOptions = false;
    response.supportsDelayedStackTraceLoading = false;
    response.supportsSetVariable = false;
    response.supportsConditionalBreakpoints = false;
    response.supportsConfigurationDoneRequest = true;
    return response;
  });

  m_session->registerSentHandler([this](const dap::ResponseOrError<dap::InitializeResponse>&) {
    m_session->send(dap::InitializedEvent{});
    TRACEARGS("[DBG] SentHandler");
  });

  m_session->registerHandler([](const dap::ThreadsRequest&) {
    TRACEARGS("[DBG] Threads request");
    dap::ThreadsResponse response;
    dap::Thread thread;
    thread.id = DBG_THREAD_ID;
    thread.name = "Main";
    response.threads.push_back(thread);
    return response;
  });

  m_session->registerHandler(
    [](const dap::AttachRequest&) -> dap::ResponseOrError<dap::AttachResponse> {
      TRACEARGS("[DBG] Received attach request");
      return dap::Error("Attaching is not supported at the moment");
    });

  m_session->registerHandler(
    [this](const AseLaunchRequest& request) -> dap::ResponseOrError<dap::LaunchResponse> {
      TRACEARGS("[DBG] Received launch request");
      if (request.noDebug)
        return dap::Error("TODO: Unsupported");

      if (m_engine)
        return dap::Error("Aseprite does not support multiple simultaneous debug sessions.");

      if ((!request.extensionName.has_value() || request.extensionName.value().empty()) &&
          (!request.scriptFilename.has_value() || request.scriptFilename.value().empty()))
        return dap::Error("Requires the extensionName or scriptFilename arguments");

      if (request.scriptFilename.has_value()) {
        std::string filename = request.scriptFilename.value();

        if (!base::is_absolute_path(filename) || base::is_file(filename)) {
          ResourceFinder rf;
          rf.includeUserDir(base::join_path("scripts", filename).c_str());
          if (rf.findFirst())
            filename = rf.filename();
        }
        filename = base::normalize_path(filename);
        if (!base::is_file(filename))
          return dap::Error("Could not find scriptFilename");

        ui::execute_from_ui_thread([this, filename] {
          m_engine = EngineManager::create();
          std::string lastError;
          m_engine->ConsoleError.connect(
            [&lastError](const std::string& error) { lastError = error; });
          m_engine->setDebugger(this);
          m_engine->evalUserFile(filename);

          if (m_engine->returnCode() < 0)
            m_session->send(dap::StoppedEvent{
              .description = "Script encountered an error",
              .reason = "exception",
              .text = lastError,
            });

          m_session->send(dap::TerminatedEvent{});
        });
      }
      else if (request.extensionName.has_value())
        return dap::Error("TODO: Extension debugging support");

      return dap::LaunchResponse{};
    });

  m_session->registerHandler([this](const dap::SetBreakpointsRequest& request)
                               -> dap::ResponseOrError<dap::SetBreakpointsResponse> {
    m_breakpoints.clear();

    if (!request.breakpoints.has_value())
      return dap::SetBreakpointsResponse{};

    if (!request.source.path.has_value())
      return dap::Error("Cannot find source for breakpoint");

    for (const auto& breakpoint : request.breakpoints.value()) {
      const auto path = base::normalize_path(request.source.path.value());
      m_breakpoints.emplace(path, breakpoint.line);
    }

    return dap::SetBreakpointsResponse{};
  });

  m_session->registerHandler(
    [this](const dap::PauseRequest&) -> dap::ResponseOrError<dap::PauseResponse> {
      if (isWaiting())
        return dap::Error("Already paused");

      setHookState(State::Wait);
      stop("pause", 0);
      return dap::PauseResponse{};
    });

  m_session->registerHandler(
    [this](const dap::ContinueRequest&) -> dap::ResponseOrError<dap::ContinueResponse> {
      if (!isWaiting())
        return dap::Error("Execution is not paused");

      setHookState(State::Pass);
      return dap::ContinueResponse{};
    });

  m_session->registerHandler(
    [this](const dap::NextRequest&) -> dap::ResponseOrError<dap::NextResponse> {
      if (!isWaiting())
        return dap::Error("Execution is not paused");

      setHookState(State::NextLine);
      return dap::NextResponse{};
    });

  m_session->registerHandler(
    [this](const dap::StepInRequest&) -> dap::ResponseOrError<dap::StepInResponse> {
      if (!isWaiting())
        return dap::Error("Execution is not paused");

      m_activeStackLevel += 1;
      setHookState(State::NextLine);
      return dap::StepInResponse{};
    });

  m_session->registerHandler(
    [this](const dap::StepOutRequest&) -> dap::ResponseOrError<dap::StepOutResponse> {
      if (!isWaiting())
        return dap::Error("Execution is not paused");

      if (m_currentStackLevel == 1 || m_activeStackLevel == 1)
        return dap::Error("Already at the top level");

      m_activeStackLevel -= 1;
      setHookState(State::NextLine);
      return dap::StepOutResponse{};
    });

  m_session->registerHandler([](const dap::DisconnectRequest&) {
    TRACEARGS("[DBG] Client closing connection");
    return dap::DisconnectResponse{};
  });

  m_session->registerHandler(
    [this](const dap::StackTraceRequest&) -> dap::ResponseOrError<dap::StackTraceResponse> {
      if (m_currentStackTrace.empty())
        return dap::StackTraceResponse{};
      return dap::StackTraceResponse{ .stackFrames = m_currentStackTrace };
    });

  m_session->registerHandler(
    [](const dap::ScopesRequest&) -> dap::ResponseOrError<dap::ScopesResponse> {
      return dap::ScopesResponse{
        .scopes = { dap::Scope{
                      .expensive = false,
                      .name = "Locals",
                      .variablesReference = INT_MAX - 1,
                    }, dap::Scope{
                      .expensive = false,
                      .name = "Globals",
                      .variablesReference = INT_MAX,
                    } }
      };
    });

  m_session->registerHandler(
    [this](const dap::VariablesRequest& request) -> dap::ResponseOrError<dap::VariablesResponse> {
      if (request.variablesReference == INT_MAX - 1) {
        return dap::VariablesResponse{
          .variables = m_localVariables,
        };
      }
      if (request.variablesReference == INT_MAX) {
        return dap::VariablesResponse{
          .variables = m_globalVariables,
        };
      }

      return dap::VariablesResponse{
        .variables = m_tableVariables[request.variablesReference - 1],
      };
    });

  m_session->registerSentHandler([this](const dap::ResponseOrError<dap::DisconnectResponse>&) {
    TRACEARGS("[DBG] Should close up things here for the session");
    setHookState(State::Pass);

    // TODO: HACK: Trying to mitigate a crash when receiving an event after deleting the engine
    // which might cause us to hook forward? Maybe a better one would be to State::Abort in case
    // we're paused?
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ui::execute_from_ui_thread([this] {
      if (m_engine) {
        m_engine->setDebugger(nullptr);
        m_engine.reset();
      }
    });
  });

  m_session->registerHandler([](const dap::ConfigurationDoneRequest&) {
    TRACEARGS("[DBG] Configuration Done");
    return dap::ConfigurationDoneResponse{};
  });

  m_session->setOnInvalidData(dap::kClose);
  m_session->bind(rw, [] { TRACEARGS("[DBG] Client connection closed"); });

  TRACEARGS("[DBG] Sending thread started event");
  dap::ThreadEvent threadStartedEvent;
  threadStartedEvent.reason = "started";
  threadStartedEvent.threadId = DBG_THREAD_ID;
  m_session->send(threadStartedEvent);
}

void Debugger::onClientError(const char* msg) const
{
  PRINTARGS("[DBG] Client error:", msg);
}

void Debugger::onSessionError(const char* msg)
{
  PRINTARGS("[DBG] Session error:", msg);
  m_session.reset();
}

} // namespace app::script
