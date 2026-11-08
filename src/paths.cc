#include "paths.h"

#include <cstdlib>

namespace {

std::filesystem::path UserConfigDir() {
#if defined(OS_WIN)
  if (const wchar_t* appdata = _wgetenv(L"APPDATA")) {
    return std::filesystem::path(appdata);
  }
#elif defined(OS_MAC)
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Library" / "Application Support";
  }
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    return std::filesystem::path(xdg);
  }
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".config";
  }
#endif
  return std::filesystem::current_path();
}

}  // namespace

std::string InstanceName(CefRefPtr<CefCommandLine> command_line) {
  std::string raw = command_line->GetSwitchValue("instance").ToString();
  std::string name;
  for (char c : raw) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_') {
      name.push_back(c);
    }
  }
  return name.empty() ? "default" : name;
}

std::filesystem::path AppDataDir() {
  return UserConfigDir() / "SchuWebRosol";
}

std::filesystem::path ConfigPath(CefRefPtr<CefCommandLine> command_line) {
  if (command_line->HasSwitch("config")) {
#if defined(OS_WIN)
    return std::filesystem::path(
        command_line->GetSwitchValue("config").ToWString());
#else
    return std::filesystem::path(
        command_line->GetSwitchValue("config").ToString());
#endif
  }
  return AppDataDir() / (InstanceName(command_line) + ".json");
}

std::filesystem::path CachePath(CefRefPtr<CefCommandLine> command_line) {
  return AppDataDir() / "cache" / InstanceName(command_line);
}
