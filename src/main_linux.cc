#include "app.h"
#include "include/cef_command_line.h"

int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  CefRefPtr<SchuApp> app(new SchuApp);

  const int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromArgv(argc, argv);
  CefSettings settings = MakeCefSettings(command_line);

  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return CefGetExitCode();
  }
  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
