// Persistent configuration of one SchuWebRosol instance.
#ifndef SCHUWEBROSOL_SETTINGS_H_
#define SCHUWEBROSOL_SETTINGS_H_

#include <filesystem>
#include <string>

#include "include/cef_command_line.h"
#include "include/cef_values.h"

struct Settings {
  std::string url = "https://threejs.org/examples/webgl_animation_keyframes.html";
  int width = 1920;
  int height = 1080;
  int fps = 60;
  std::string sender_name = "SchuWebRosol";
  bool transparent = false;
  bool mute = true;
  int zoom_percent = 100;
  bool hide_scrollbars = true;
  int reload_interval = 0;  // Seconds, 0 = never.
  std::string custom_css;
  std::string custom_js;

  void Clamp();

  CefRefPtr<CefDictionaryValue> ToDict() const;
  // Applies only the keys present in |dict|.
  void Merge(CefRefPtr<CefDictionaryValue> dict);
  // Applies --url, --width, --height, --fps, --name, --transparent, ...
  void Merge(CefRefPtr<CefCommandLine> command_line);

  bool Load(const std::filesystem::path& path);
  bool Save(const std::filesystem::path& path) const;
};

#endif  // SCHUWEBROSOL_SETTINGS_H_
