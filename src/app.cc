#include "app.h"

#include "controller.h"
#include "include/wrapper/cef_helpers.h"
#include "paths.h"

void SchuApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  if (!process_type.empty()) {
    return;
  }
  // Media on VJ pages should start without a click.
  command_line->AppendSwitchWithValue("autoplay-policy",
                                      "no-user-gesture-required");
  // The output browser is never visible on screen; keep it running at full
  // speed anyway.
  command_line->AppendSwitch("disable-background-timer-throttling");
  command_line->AppendSwitch("disable-renderer-backgrounding");
  command_line->AppendSwitch("disable-backgrounding-occluded-windows");
#if defined(OS_MAC)
  command_line->AppendSwitch("use-mock-keychain");
#endif
}

void SchuApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();
  Controller::Get()->Start(CefCommandLine::GetGlobalCommandLine());
}

CefSettings MakeCefSettings(CefRefPtr<CefCommandLine> command_line) {
  CefSettings settings;
  settings.no_sandbox = true;
  settings.windowless_rendering_enabled = true;
  settings.persist_session_cookies = true;
  settings.log_severity = command_line->HasSwitch("verbose")
                              ? LOGSEVERITY_INFO
                              : LOGSEVERITY_WARNING;

  const auto cache = CachePath(command_line);
  CefString(&settings.root_cache_path) = cache.native();
  CefString(&settings.cache_path) = cache.native();
  const auto log =
      AppDataDir() / (InstanceName(command_line) + ".log");
  CefString(&settings.log_file) = log.native();
  return settings;
}
