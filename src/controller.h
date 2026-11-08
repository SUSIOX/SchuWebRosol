// Owns the settings, the output (off-screen) browser, the frame sender and
// the optional control window. Methods marked "UI thread" must be called on
// the CEF UI thread; the others are thread-safe.
#ifndef SCHUWEBROSOL_CONTROLLER_H_
#define SCHUWEBROSOL_CONTROLLER_H_

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "frame_sender.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_values.h"
#include "settings.h"

class OsrClient;

class Controller {
 public:
  static Controller* Get();

  // UI thread.
  void Start(CefRefPtr<CefCommandLine> command_line);
  void ApplySettings(const Settings& next);
  void Reload();
  void Click(double x, double y);
  void Scroll(int delta_y);
  void TypeText(const std::string& text);
  void PressKey(const std::string& key);
  void CloseAll();

  // UI thread, called by the browser clients.
  void OnBrowserCreated();
  void OnBrowserClosed();
  void OnControlCreated(CefRefPtr<CefBrowser> browser);
  void OnControlClosed();
  void OnOutputCreated(OsrClient* client);
  void OnOutputAddressChange(OsrClient* client, const std::string& url);
  void OnOutputLoadingState(OsrClient* client, bool loading);
  void OnOutputLoadEnd(OsrClient* client,
                       CefRefPtr<CefFrame> frame,
                       int http_status);
  void OnOutputLoadError(OsrClient* client, const std::string& error);
  void OnFrame(OsrClient* client, const void* bgra, int width, int height);

  // Any thread.
  Settings GetSettings();
  CefRefPtr<CefDictionaryValue> GetStatus();
  // Downscaled preview of the latest frame as a 24-bit BMP file.
  std::string GetPreviewBmp();

 private:
  Controller() = default;

  void CreateOutputBrowser();
  void CreateControlWindow();
  void Tick();
  void InjectPageTweaks(CefRefPtr<CefFrame> frame);
  void SendKey(int windows_key_code, int native_key_code, char16_t character);
  CefRefPtr<CefBrowserHost> OutputHost();

  std::mutex mutex_;
  Settings settings_;
  std::filesystem::path config_path_;
  std::string instance_;
  bool headless_ = false;

  std::unique_ptr<FrameSender> sender_;
  CefRefPtr<OsrClient> output_;
  CefRefPtr<CefBrowser> control_browser_;
  int live_browsers_ = 0;
  bool closing_ = false;

  // Status (guarded by |mutex_|).
  int frames_this_second_ = 0;
  int measured_fps_ = 0;
  int frame_width_ = 0;
  int frame_height_ = 0;
  bool loading_ = false;
  std::string current_url_;
  std::string last_error_;
  int seconds_since_reload_ = 0;

  // Preview (guarded by |mutex_|).
  std::vector<unsigned char> preview_;
  int preview_width_ = 0;
  int preview_height_ = 0;
  std::chrono::steady_clock::time_point preview_wanted_until_;
};

#endif  // SCHUWEBROSOL_CONTROLLER_H_
