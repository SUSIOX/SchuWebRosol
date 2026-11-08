// Per-user locations for configuration and browser cache.
#ifndef SCHUWEBROSOL_PATHS_H_
#define SCHUWEBROSOL_PATHS_H_

#include <filesystem>
#include <string>

#include "include/cef_command_line.h"

// Instance name from --instance (sanitized), "default" when absent.
std::string InstanceName(CefRefPtr<CefCommandLine> command_line);

// <user config dir>/SchuWebRosol
std::filesystem::path AppDataDir();

// --config=<file> or <AppDataDir>/<instance>.json
std::filesystem::path ConfigPath(CefRefPtr<CefCommandLine> command_line);

// <AppDataDir>/cache/<instance>
std::filesystem::path CachePath(CefRefPtr<CefCommandLine> command_line);

#endif  // SCHUWEBROSOL_PATHS_H_
