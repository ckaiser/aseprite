// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/app.h"
#include "app/resource_finder.h"
#include "app/script/debugger.h"
#include "base/exception.h"
#include "base/fs.h"
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

// Arbitrary "main thread" ID for cppdap
#define DBG_THREAD_ID 100

struct AseLaunchRequest : dap::LaunchRequest {
  using Response = dap::LaunchResponse;
  dap::optional<dap::string> extensionName;
  dap::optional<dap::string> scriptFilename;
};

struct AseAttachRequest : dap::AttachRequest {
  using Response = dap::AttachResponse;
  dap::optional<dap::string> extensionName;
};

namespace dap {
DAP_STRUCT_TYPEINFO_EXT(AseLaunchRequest,
                        LaunchRequest,
                        "launch",
                        DAP_FIELD(extensionName, "extensionName"),
                        DAP_FIELD(scriptFilename, "scriptFilename"));
DAP_STRUCT_TYPEINFO_EXT(AseAttachRequest,
                        AttachRequest,
                        "attach",
                        DAP_FIELD(extensionName, "extensionName"));
} // namespace dap

namespace {
// Helper function to extract the key of a table while iterating it through lua_next
std::string next_key_string(lua_State* L)
{
  lua_pushvalue(L, -2);
  std::string key;
  const int keyType = lua_type(L, -1);
  if (keyType == LUA_TSTRING || keyType == LUA_TNUMBER) {
    key = lua_tostring(L, -1);
  }
  else if (keyType == LUA_TBOOLEAN) {
    key = fmt::format("[bool:{}]", lua_toboolean(L, -1) ? "true" : "false");
  }
  else {
    key = fmt::format("[{}]", lua_typename(L, keyType));
  }
  lua_pop(L, 1);
  return key;
}
} // namespace
namespace app::script {

ui::RegisterMessage kDebuggerWakeMessage;

Debugger::Debugger(const std::string& address, const uint16_t port)
{
  m_server = dap::net::Server::create();
  const bool result = m_server->start(
    address.c_str(),
    port,
    [this](const auto& rw) { onClientConnected(rw); },
    [this](const char* msg) {
      Error(fmt::format("Debugger error: {}", msg));
      m_session.reset();
    });
  if (!result)
    throw base::Exception("Could not create debugger server.");

  m_changeConn = App::instance()->extensions().ScriptsChange.connect(&Debugger::onExtensionChange,
                                                                     this);
}

Debugger::~Debugger()
{
  ASSERT(!isDebugging());
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

  // Updating locals
  m_localVariables.clear();
  m_visitedVariables.clear();
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
  m_visitedVariables.clear();
  lua_pushglobaltable(L);
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    m_globalVariables.push_back(makeVariable(L, next_key_string(L)));
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
}

void Debugger::updateStackTrace(lua_State* L)
{
  m_currentStackTrace.clear();
  lua_Debug ar;
  int level = 0;
  while (lua_getstack(L, level++, &ar)) {
    dap::StackFrame frame;
    frame.id = level;
    lua_getinfo(L, "Slntf", &ar);
    const std::string filename(ar.short_src, ar.srclen - 1);

    if (!filename.empty() && filename != "[C]") {
      dap::Source source;
      source.path = filename;
      frame.source = source;
    }

    if (ar.currentline > 0)
      frame.line = ar.currentline;

    // If we're inside a pcall we lose ar.name
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
  dap::Variable var;
  var.name = name;

  // Avoid stepping through _G or _ENV so globals expand properly.
  if (name == "_G" || name == "_ENV")
    return var;

  // Because we're using a global m_visitedVariables, this is a bit of a hacky way to avoid "top
  // level" variables from having a "duplicate reference" value when they're a reference to another
  // variable in a way that's not really recursive.
  static size_t level = 0;
  ++level;

  // Right now we're eagerly evaluating both the global and local scope, since Lua tables can
  // reference themselves recursively we need to keep track of visits to avoid infinite loops.
  // Ideally we should be just using dap::Variables variablesReference to lazily load the values
  // when the debugger requests it, but I'm not entirely sure how that would play with our threading
  // model.
  const void* ptr = lua_topointer(L, -1);
  if (level > 1 && ptr) {
    if (m_visitedVariables.find(ptr) == m_visitedVariables.end())
      m_visitedVariables.insert(ptr);
    else {
      var.value = fmt::format("[duplicate reference: {}]", ptr);
      --level;
      return var;
    }
  }
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
      lua_pushnil(L);
      size_t count = 0;
      while (lua_next(L, -2) != 0) {
        count++;
        variables.push_back(makeVariable(L, next_key_string(L)));
        lua_pop(L, 1);
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

  --level;
  return var;
}

// Sends the stop event and sets the hook state to "wait"
// Valid reasons:
// 'step', 'breakpoint', 'exception', 'pause', 'entry', 'goto', 'function breakpoint',
// 'data breakpoint', 'instruction breakpoint'
void Debugger::stop(const std::string& reason, const dap::integer line)
{
  dap::StoppedEvent event;
  event.reason = reason;
  event.threadId = DBG_THREAD_ID;

  if (line > 0)
    event.hitBreakpointIds = { line };
  m_session->send(event);
  setHookState(State::Wait);
}

void Debugger::onHook(lua_State* L, lua_Debug* ar)
{
  ASSERT(isDebugging());
  ASSERT(m_session);
  ASSERT(ar);

  if (m_hookState.load() == State::Abort)
    return;

  int ret = lua_getinfo(L, "lSn", ar);
  if (ret == 0 || ar->currentline < 0)
    return;

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

      std::string filename = ar->source;
      if (!filename.empty() && filename[0] == '@')
        filename = ar->source + 1;
      filename = base::normalize_path(filename);

      const std::lock_guard lock(m_mutex);
      for (auto [itr, rangeEnd] = m_breakpoints.equal_range(filename); itr != rangeEnd; ++itr) {
        if (itr->second == ar->currentline) {
          stop("breakpoint", ar->currentline);
          break;
        }
      }
      break;
  }

  if (m_hookState.load() == State::Wait) {
    {
      const std::lock_guard lock(m_mutex);
      updateVariables(L);
      updateStackTrace(L);
    }

    ui::MessageLoop loop(ui::Manager::getDefault());
    while (m_hookState.load() == State::Wait)
      loop.pumpMessages();
  }
}

void Debugger::onExtensionChange(Extension* ext)
{
  if (!m_extension)
    return;

  if (ext == nullptr || m_extension == ext) {
    // If scripts have changed in any way we must terminate the debugging session to avoid having a
    // dangling Extension pointer.
    m_session->send(dap::TerminatedEvent{});
  }
}

void Debugger::onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw)
{
  m_session = dap::Session::create();
  m_session->onError(
    [this](const char* msg) { Error(fmt::format("Debugger session error: {}", msg)); });

  m_session->registerHandler([](const dap::InitializeRequest&) {
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
  });

  m_session->registerHandler([](const dap::ThreadsRequest&) {
    dap::ThreadsResponse response;
    dap::Thread thread;
    thread.id = DBG_THREAD_ID;
    thread.name = "Main";
    response.threads.push_back(thread);
    return response;
  });

  m_session->registerHandler([this](const AseAttachRequest& request)
                               -> dap::ResponseOrError<dap::AttachResponse> {
    if (isDebugging())
      return dap::Error("Aseprite does not support multiple simultaneous debug sessions.");

    if (!request.extensionName.has_value() || request.extensionName.value().empty())
      return dap::Error("Requires the extensionName arguments");

    for (Extension* ext : App::instance()->extensions()) {
      if (ext->name() == request.extensionName.value()) {
        if (!ext->isEnabled())
          return dap::Error(
            "The Extension is not currently enabled, enable it inside Aseprite or use the 'launch' request.");

        if (!ext->hasScripts())
          return dap::Error("The Extension does not have any scripts to debug.");

        m_extension = ext;
        setHookState(State::Pass);
        ui::execute_from_ui_thread([this] { m_extension->scriptEngine()->setDebugger(this); });

        return dap::AttachResponse{};
      }
    }

    return dap::Error("Extension not found");
  });

  m_session->registerHandler(
    [this](const AseLaunchRequest& request) -> dap::ResponseOrError<dap::LaunchResponse> {
      if (request.noDebug)
        return dap::Error("'No debug' option is unsupported");

      if (isDebugging())
        return dap::Error("Aseprite does not support multiple simultaneous debug sessions.");

      if ((!request.extensionName.has_value() || request.extensionName.value().empty()) &&
          (!request.scriptFilename.has_value() || request.scriptFilename.value().empty()))
        return dap::Error("Requires the extensionName or scriptFilename arguments");

      if (request.scriptFilename.has_value() && !request.scriptFilename.value().empty()) {
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
          setHookState(State::Pass);
          m_engine = EngineManager::create();
          std::string lastError;
          m_engine->ConsoleError.connect(
            [&lastError](const std::string& error) { lastError = error; });
          m_engine->setDebugger(this);
          m_engine->evalUserFile(filename);

          if (m_engine->returnCode() < 0) {
            dap::StoppedEvent event;
            event.description = "Script encountered an error";
            event.reason = "exception";
            event.text = lastError;
            m_session->send(event);
          }

          m_engine.reset();
          m_session->send(dap::TerminatedEvent{});
        });
      }
      else if (request.extensionName.has_value() && !request.extensionName.value().empty()) {
        for (Extension* ext : App::instance()->extensions()) {
          if (ext->name() == request.extensionName.value()) {
            if (!ext->hasScripts())
              return dap::Error("The Extension does not have any scripts to debug.");

            m_extension = ext;
            ui::execute_from_ui_thread([this] {
              if (m_extension->isEnabled())
                App::instance()->extensions().enableExtension(m_extension, false);

              setHookState(State::Pass);

              // Pre-initialize the engine with the debugger already created, so that we can
              // set breakpoints inside init().
              m_extension->m_engine = EngineManager::create();
              m_extension->m_engine->setDebugger(this);
              App::instance()->extensions().enableExtension(m_extension, true);
            });
          }
        }
      }

      return dap::LaunchResponse{};
    });

  m_session->registerHandler([this](const dap::SetBreakpointsRequest& request)
                               -> dap::ResponseOrError<dap::SetBreakpointsResponse> {
    const std::lock_guard lock(m_mutex);
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

  m_session->registerHandler(
    [](const dap::DisconnectRequest&) { return dap::DisconnectResponse{}; });

  m_session->registerHandler(
    [this](const dap::StackTraceRequest&) -> dap::ResponseOrError<dap::StackTraceResponse> {
      const std::lock_guard lock(m_mutex);
      dap::StackTraceResponse response;
      response.stackFrames = m_currentStackTrace;
      return response;
    });

  m_session->registerHandler(
    [](const dap::ScopesRequest&) -> dap::ResponseOrError<dap::ScopesResponse> {
      dap::ScopesResponse response;
      dap::Scope locals;
      locals.expensive = false;
      locals.name = "Locals";
      locals.variablesReference = INT_MAX - 1;

      dap::Scope globals;
      locals.expensive = false;
      locals.name = "Globals";
      locals.variablesReference = INT_MAX;

      response.scopes = { locals, globals };

      return response;
    });

  m_session->registerHandler(
    [this](const dap::VariablesRequest& request) -> dap::ResponseOrError<dap::VariablesResponse> {
      const std::lock_guard lock(m_mutex);

      dap::VariablesResponse response;
      if (request.variablesReference == INT_MAX - 1) {
        response.variables = m_localVariables;
      }
      else if (request.variablesReference == INT_MAX) {
        response.variables = m_globalVariables;
      }
      else {
        response.variables = m_tableVariables[request.variablesReference - 1];
      }

      return response;
    });

  m_session->registerHandler(
    [](const dap::ConfigurationDoneRequest&) { return dap::ConfigurationDoneResponse{}; });

  m_session->setOnInvalidData(dap::kClose);
  m_session->bind(rw, [this] {
    setHookState(State::Abort);

    ui::execute_from_ui_thread([this] {
      if (m_engine)
        m_engine->setDebugger(nullptr);
      if (m_extension) {
        if (m_extension->scriptEngine())
          m_extension->scriptEngine()->setDebugger(nullptr);
        m_extension = nullptr;
      }
    });
  });

  dap::ThreadEvent threadStartedEvent;
  threadStartedEvent.reason = "started";
  threadStartedEvent.threadId = DBG_THREAD_ID;
  m_session->send(threadStartedEvent);
}

} // namespace app::script
