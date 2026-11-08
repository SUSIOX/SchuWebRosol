#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#include "include/cef_parser.h"

namespace {

int GetInt(CefRefPtr<CefDictionaryValue> dict, const char* key, int fallback) {
  switch (dict->GetType(key)) {
    case VTYPE_INT:
      return dict->GetInt(key);
    case VTYPE_DOUBLE:
      return static_cast<int>(dict->GetDouble(key));
    case VTYPE_STRING:
      return std::atoi(dict->GetString(key).ToString().c_str());
    default:
      return fallback;
  }
}

bool GetBool(CefRefPtr<CefDictionaryValue> dict, const char* key,
             bool fallback) {
  switch (dict->GetType(key)) {
    case VTYPE_BOOL:
      return dict->GetBool(key);
    case VTYPE_INT:
      return dict->GetInt(key) != 0;
    default:
      return fallback;
  }
}

std::string GetString(CefRefPtr<CefDictionaryValue> dict, const char* key,
                      const std::string& fallback) {
  if (dict->GetType(key) != VTYPE_STRING) {
    return fallback;
  }
  return dict->GetString(key).ToString();
}

int SwitchInt(CefRefPtr<CefCommandLine> cl, const char* name, int fallback) {
  if (!cl->HasSwitch(name)) {
    return fallback;
  }
  return std::atoi(cl->GetSwitchValue(name).ToString().c_str());
}

bool SwitchBool(CefRefPtr<CefCommandLine> cl, const char* name, bool fallback) {
  if (!cl->HasSwitch(name)) {
    return fallback;
  }
  const std::string value = cl->GetSwitchValue(name).ToString();
  return !(value == "0" || value == "false" || value == "no" || value == "off");
}

}  // namespace

void Settings::Clamp() {
  width = std::clamp(width, 16, 8192);
  height = std::clamp(height, 16, 8192);
  fps = std::clamp(fps, 1, 240);
  zoom_percent = std::clamp(zoom_percent, 25, 500);
  reload_interval = std::clamp(reload_interval, 0, 7 * 24 * 3600);
  if (sender_name.empty()) {
    sender_name = "SchuWebRosol";
  }
  if (sender_name.size() > 200) {
    sender_name.resize(200);
  }
  if (url.empty()) {
    url = "about:blank";
  }
}

CefRefPtr<CefDictionaryValue> Settings::ToDict() const {
  CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
  dict->SetString("url", url);
  dict->SetInt("width", width);
  dict->SetInt("height", height);
  dict->SetInt("fps", fps);
  dict->SetString("sender_name", sender_name);
  dict->SetBool("transparent", transparent);
  dict->SetBool("mute", mute);
  dict->SetInt("zoom_percent", zoom_percent);
  dict->SetBool("hide_scrollbars", hide_scrollbars);
  dict->SetInt("reload_interval", reload_interval);
  dict->SetString("custom_css", custom_css);
  dict->SetString("custom_js", custom_js);
  return dict;
}

void Settings::Merge(CefRefPtr<CefDictionaryValue> dict) {
  if (!dict) {
    return;
  }
  url = GetString(dict, "url", url);
  width = GetInt(dict, "width", width);
  height = GetInt(dict, "height", height);
  fps = GetInt(dict, "fps", fps);
  sender_name = GetString(dict, "sender_name", sender_name);
  transparent = GetBool(dict, "transparent", transparent);
  mute = GetBool(dict, "mute", mute);
  zoom_percent = GetInt(dict, "zoom_percent", zoom_percent);
  hide_scrollbars = GetBool(dict, "hide_scrollbars", hide_scrollbars);
  reload_interval = GetInt(dict, "reload_interval", reload_interval);
  custom_css = GetString(dict, "custom_css", custom_css);
  custom_js = GetString(dict, "custom_js", custom_js);
  Clamp();
}

void Settings::Merge(CefRefPtr<CefCommandLine> cl) {
  if (cl->HasSwitch("url")) {
    url = cl->GetSwitchValue("url").ToString();
  }
  if (cl->HasSwitch("name")) {
    sender_name = cl->GetSwitchValue("name").ToString();
  }
  width = SwitchInt(cl, "width", width);
  height = SwitchInt(cl, "height", height);
  fps = SwitchInt(cl, "fps", fps);
  zoom_percent = SwitchInt(cl, "zoom", zoom_percent);
  reload_interval = SwitchInt(cl, "reload-interval", reload_interval);
  transparent = SwitchBool(cl, "transparent", transparent);
  mute = SwitchBool(cl, "mute", mute);
  hide_scrollbars = SwitchBool(cl, "hide-scrollbars", hide_scrollbars);
  Clamp();
}

bool Settings::Load(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  CefRefPtr<CefValue> value =
      CefParseJSON(buffer.str(), JSON_PARSER_ALLOW_TRAILING_COMMAS);
  if (!value || value->GetType() != VTYPE_DICTIONARY) {
    return false;
  }
  Merge(value->GetDictionary());
  return true;
}

bool Settings::Save(const std::filesystem::path& path) const {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(ToDict());
  const std::string json =
      CefWriteJSON(value, JSON_WRITER_PRETTY_PRINT).ToString();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << json;
  return static_cast<bool>(out);
}
