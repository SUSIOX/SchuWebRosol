#ifndef SCHUWEBROSOL_APP_H_
#define SCHUWEBROSOL_APP_H_

#include "include/cef_app.h"

class SchuApp : public CefApp, public CefBrowserProcessHandler {
 public:
  SchuApp() = default;

  // CefApp
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override;

  // CefBrowserProcessHandler
  void OnContextInitialized() override;

 private:
  IMPLEMENT_REFCOUNTING(SchuApp);
};

// Builds CefSettings (cache/log location per instance) for the browser
// process.
CefSettings MakeCefSettings(CefRefPtr<CefCommandLine> command_line);

#endif  // SCHUWEBROSOL_APP_H_
