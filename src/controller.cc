#include "controller.h"

#include <cmath>
#include <cstdio>

#include "control_window.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "osr_client.h"
#include "paths.h"

namespace {

constexpr int kPreviewMaxWidth = 640;

double ZoomLevelFromPercent(int percent) {
  return std::log(percent / 100.0) / std::log(1.2);
}

std::string JsString(const std::string& value) {
  CefRefPtr<CefValue> v = CefValue::Create();
  v->SetString(value);
  return CefWriteJSON(v, JSON_WRITER_DEFAULT).ToString();
}

void PutLe(std::string& out, uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) {
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

}  // namespace

Controller* Controller::Get() {
  static Controller* instance = new Controller();
  return instance;
}

void Controller::Start(CefRefPtr<CefCommandLine> command_line) {
  CEF_REQUIRE_UI_THREAD();
  instance_ = InstanceName(command_line);
  config_path_ = ConfigPath(command_line);
  headless_ = command_line->HasSwitch("no-ui");

  Settings settings;
  if (instance_ != "default") {
    settings.sender_name = "SchuWebRosol " + instance_;
  }
  settings.Load(config_path_);
  settings.Merge(command_line);
  settings.Save(config_path_);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
  }

  sender_ = FrameSender::Create(settings.sender_name);
  std::printf("SchuWebRosol: instance '%s', %s sender '%s', config %s\n",
              instance_.c_str(), sender_->Backend().c_str(),
              settings.sender_name.c_str(), config_path_.string().c_str());
  std::fflush(stdout);

  CreateOutputBrowser();
  if (!headless_) {
    CreateControlWindow();
  }
  CefPostDelayedTask(
      TID_UI, base::BindOnce(&Controller::Tick, base::Unretained(this)), 1000);
}

void Controller::CreateOutputBrowser() {
  const Settings s = GetSettings();
  output_ = new OsrClient(s.width, s.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(kNullWindowHandle);

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = s.fps;
  browser_settings.background_color =
      s.transparent ? CefColorSetARGB(0, 0, 0, 0)
                    : CefColorSetARGB(255, 255, 255, 255);

  CefBrowserHost::CreateBrowser(window_info, output_, s.url, browser_settings,
                                nullptr, nullptr);
}

void Controller::CreateControlWindow() {
  ShowControlWindow(instance_ == "default"
                        ? std::string("SchuWebRosol")
                        : "SchuWebRosol - " + instance_);
}

CefRefPtr<CefBrowserHost> Controller::OutputHost() {
  if (output_ && output_->browser()) {
    return output_->browser()->GetHost();
  }
  return nullptr;
}

void Controller::ApplySettings(const Settings& requested) {
  CEF_REQUIRE_UI_THREAD();
  Settings next = requested;
  next.Clamp();
  Settings prev;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    prev = settings_;
    settings_ = next;
    seconds_since_reload_ = 0;
  }
  next.Save(config_path_);

  if (next.sender_name != prev.sender_name && sender_) {
    sender_->SetName(next.sender_name);
  }
  if (!output_ || closing_) {
    return;
  }
  if (next.transparent != prev.transparent) {
    // The background color can only be set when the browser is created.
    output_->Close();
    output_ = nullptr;
    CreateOutputBrowser();
    return;
  }
  CefRefPtr<CefBrowserHost> host = OutputHost();
  if (!host) {
    return;
  }
  if (next.width != prev.width || next.height != prev.height) {
    output_->SetSize(next.width, next.height);
  }
  if (next.fps != prev.fps) {
    host->SetWindowlessFrameRate(next.fps);
  }
  if (next.mute != prev.mute) {
    host->SetAudioMuted(next.mute);
  }
  if (next.zoom_percent != prev.zoom_percent) {
    host->SetZoomLevel(ZoomLevelFromPercent(next.zoom_percent));
  }
  CefRefPtr<CefFrame> frame = host->GetBrowser()->GetMainFrame();
  if (next.url != prev.url) {
    frame->LoadURL(next.url);
  } else {
    if (next.custom_css != prev.custom_css ||
        next.hide_scrollbars != prev.hide_scrollbars) {
      InjectPageTweaks(frame);
    }
    if (next.custom_js != prev.custom_js && !next.custom_js.empty()) {
      frame->ExecuteJavaScript(next.custom_js, frame->GetURL(), 0);
    }
  }
}

void Controller::InvalidateOutput() {
  CEF_REQUIRE_UI_THREAD();
  if (CefRefPtr<CefBrowserHost> host = OutputHost()) {
    host->Invalidate(PET_VIEW);
  }
}

void Controller::Reload() {
  CEF_REQUIRE_UI_THREAD();
  if (output_ && output_->browser()) {
    output_->browser()->ReloadIgnoreCache();
  }
}

void Controller::Click(double x, double y) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowserHost> host = OutputHost();
  if (!host) {
    return;
  }
  CefMouseEvent event;
  event.x = static_cast<int>(x * output_->width());
  event.y = static_cast<int>(y * output_->height());
  host->SetFocus(true);
  host->SendMouseMoveEvent(event, false);
  host->SendMouseClickEvent(event, MBT_LEFT, false, 1);
  host->SendMouseClickEvent(event, MBT_LEFT, true, 1);
}

void Controller::Scroll(int delta_y) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowserHost> host = OutputHost();
  if (!host) {
    return;
  }
  CefMouseEvent event;
  event.x = output_->width() / 2;
  event.y = output_->height() / 2;
  host->SendMouseWheelEvent(event, 0, delta_y);
}

void Controller::SendKey(int windows_key_code,
                         int native_key_code,
                         char16_t character) {
  CefRefPtr<CefBrowserHost> host = OutputHost();
  if (!host) {
    return;
  }
  host->SetFocus(true);
  CefKeyEvent event;
  event.windows_key_code = windows_key_code;
  event.native_key_code = native_key_code;
  event.character = character;
  event.unmodified_character = character;
  if (windows_key_code) {
    event.type = KEYEVENT_RAWKEYDOWN;
    host->SendKeyEvent(event);
  }
  if (character) {
    event.type = KEYEVENT_CHAR;
#if !defined(OS_MAC)
    if (!windows_key_code) {
      event.windows_key_code = character;
    }
#endif
    host->SendKeyEvent(event);
  }
  if (windows_key_code) {
    event.type = KEYEVENT_KEYUP;
    host->SendKeyEvent(event);
  }
}

void Controller::TypeText(const std::string& text) {
  CEF_REQUIRE_UI_THREAD();
  const std::u16string utf16 = CefString(text).ToString16();
  for (char16_t c : utf16) {
    SendKey(0, 0, c);
  }
}

void Controller::PressKey(const std::string& key) {
  CEF_REQUIRE_UI_THREAD();
  struct KeyInfo {
    const char* name;
    int windows_code;
    int mac_code;
    char16_t character;
  };
  static const KeyInfo kKeys[] = {
      {"Enter", 0x0D, 36, u'\r'},     {"Tab", 0x09, 48, u'\t'},
      {"Backspace", 0x08, 51, 0x08},  {"Escape", 0x1B, 53, 0},
      {"Space", 0x20, 49, u' '},      {"PageUp", 0x21, 116, 0},
      {"PageDown", 0x22, 121, 0},     {"ArrowLeft", 0x25, 123, 0},
      {"ArrowUp", 0x26, 126, 0},      {"ArrowRight", 0x27, 124, 0},
      {"ArrowDown", 0x28, 125, 0},    {"F5", 0x74, 96, 0},
  };
  for (const KeyInfo& info : kKeys) {
    if (key == info.name) {
#if defined(OS_MAC)
      SendKey(info.windows_code, info.mac_code, info.character);
#else
      SendKey(info.windows_code, 0, info.character);
#endif
      return;
    }
  }
}

void Controller::CloseAll() {
  CEF_REQUIRE_UI_THREAD();
  closing_ = true;
  if (output_) {
    output_->Close();
  }
  if (control_browser_) {
    control_browser_->GetHost()->CloseBrowser(false);
  }
  if (live_browsers_ == 0) {
    CefQuitMessageLoop();
  }
}

void Controller::OnBrowserCreated() {
  ++live_browsers_;
}

void Controller::OnBrowserClosed() {
  --live_browsers_;
  if (closing_ && live_browsers_ <= 0) {
    sender_.reset();
    CefQuitMessageLoop();
  }
}

void Controller::OnControlCreated(CefRefPtr<CefBrowser> browser) {
  control_browser_ = browser;
}

void Controller::OnControlClosed() {
  control_browser_ = nullptr;
  if (!closing_) {
    CloseAll();
  }
}

void Controller::OnOutputCreated(OsrClient* client) {
  if (client != output_.get()) {
    return;
  }
  LOG(INFO) << "Output browser created";
  const Settings s = GetSettings();
  CefRefPtr<CefBrowserHost> host = OutputHost();
  host->SetAudioMuted(s.mute);
  host->SetZoomLevel(ZoomLevelFromPercent(s.zoom_percent));
  host->WasHidden(false);
  host->SetFocus(true);
}

void Controller::OnOutputAddressChange(OsrClient* client,
                                       const std::string& url) {
  if (client != output_.get()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  current_url_ = url;
}

void Controller::OnOutputLoadingState(OsrClient* client, bool loading) {
  if (client != output_.get()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  loading_ = loading;
  if (loading) {
    last_error_.clear();
  }
}

void Controller::OnOutputLoadEnd(OsrClient* client,
                                 CefRefPtr<CefFrame> frame,
                                 int http_status) {
  if (client != output_.get()) {
    return;
  }
  LOG(INFO) << "Loaded " << frame->GetURL().ToString() << " (HTTP "
            << http_status << ")";
  const Settings s = GetSettings();
  CefRefPtr<CefBrowserHost> host = OutputHost();
  const double zoom = ZoomLevelFromPercent(s.zoom_percent);
  if (host && std::fabs(host->GetZoomLevel() - zoom) > 0.001) {
    host->SetZoomLevel(zoom);
  }
  InjectPageTweaks(frame);
  if (!s.custom_js.empty()) {
    frame->ExecuteJavaScript(s.custom_js, frame->GetURL(), 0);
  }
}

void Controller::OnOutputLoadError(OsrClient* client,
                                   const std::string& error) {
  if (client != output_.get()) {
    return;
  }
  LOG(WARNING) << "Load error: " << error;
  std::lock_guard<std::mutex> lock(mutex_);
  last_error_ = error;
}

void Controller::InjectPageTweaks(CefRefPtr<CefFrame> frame) {
  const Settings s = GetSettings();
  std::string css = s.custom_css;
  if (s.hide_scrollbars) {
    css =
        "html,body{scrollbar-width:none!important}"
        "::-webkit-scrollbar{display:none!important}\n" +
        css;
  }
  const std::string js =
      "(function(){var id='__schuwebrosol_css';"
      "var s=document.getElementById(id);"
      "if(!s){s=document.createElement('style');s.id=id;"
      "(document.head||document.documentElement).appendChild(s);}"
      "s.textContent=" +
      JsString(css) + ";})();";
  frame->ExecuteJavaScript(js, frame->GetURL(), 0);
}

void Controller::OnFrame(OsrClient* client,
                         const void* bgra,
                         int width,
                         int height) {
  if (client != output_.get() || closing_) {
    return;
  }
  if (sender_) {
    sender_->Send(bgra, width, height);
  }

  std::lock_guard<std::mutex> lock(mutex_);
  ++frames_this_second_;
  if (width != frame_width_ || height != frame_height_) {
    LOG(INFO) << "Frame size " << width << "x" << height;
  }
  frame_width_ = width;
  frame_height_ = height;
  if (std::chrono::steady_clock::now() > preview_wanted_until_) {
    return;
  }
  const int pw = std::min(width, kPreviewMaxWidth);
  const int ph = std::max(1, height * pw / width);
  preview_.resize(static_cast<size_t>(pw) * ph * 4);
  const auto* src = static_cast<const uint32_t*>(bgra);
  auto* dst = reinterpret_cast<uint32_t*>(preview_.data());
  for (int y = 0; y < ph; ++y) {
    const uint32_t* row = src + static_cast<size_t>(y * height / ph) * width;
    for (int x = 0; x < pw; ++x) {
      dst[static_cast<size_t>(y) * pw + x] = row[x * width / pw];
    }
  }
  preview_width_ = pw;
  preview_height_ = ph;
}

void Controller::Tick() {
  CEF_REQUIRE_UI_THREAD();
  if (closing_) {
    return;
  }
  bool reload = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    measured_fps_ = frames_this_second_;
    frames_this_second_ = 0;
    if (settings_.reload_interval > 0 &&
        ++seconds_since_reload_ >= settings_.reload_interval) {
      seconds_since_reload_ = 0;
      reload = true;
    }
  }
  if (reload) {
    Reload();
  }
  CefPostDelayedTask(
      TID_UI, base::BindOnce(&Controller::Tick, base::Unretained(this)), 1000);
}

Settings Controller::GetSettings() {
  std::lock_guard<std::mutex> lock(mutex_);
  return settings_;
}

CefRefPtr<CefDictionaryValue> Controller::GetStatus() {
  CefRefPtr<CefDictionaryValue> status = CefDictionaryValue::Create();
  std::lock_guard<std::mutex> lock(mutex_);
  status->SetDictionary("settings", settings_.ToDict());
  status->SetString("instance", instance_);
  status->SetString("config_path", config_path_.string());
  status->SetString("backend", sender_ ? sender_->Backend() : "");
  status->SetInt("clients", sender_ ? sender_->ClientCount() : -1);
  status->SetInt("fps", measured_fps_);
  status->SetInt("frame_width", frame_width_);
  status->SetInt("frame_height", frame_height_);
  status->SetBool("loading", loading_);
  status->SetString("current_url", current_url_);
  status->SetString("last_error", last_error_);
  status->SetString("version", SCHUWEBROSOL_VERSION);
  return status;
}

std::string Controller::GetPreviewBmp() {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::steady_clock::now();
  if (now > preview_wanted_until_) {
    // Frames painted while nobody watched were not copied; static pages
    // would never repaint on their own.
    CefPostTask(TID_UI, base::BindOnce(&Controller::InvalidateOutput,
                                       base::Unretained(this)));
  }
  preview_wanted_until_ = now + std::chrono::seconds(3);
  if (preview_.empty()) {
    return std::string();
  }
  const int w = preview_width_;
  const int h = preview_height_;
  const bool checker = settings_.transparent;
  const int row_size = (w * 3 + 3) & ~3;
  const uint32_t image_size = static_cast<uint32_t>(row_size) * h;

  std::string bmp;
  bmp.reserve(54 + image_size);
  bmp += "BM";
  PutLe(bmp, 54 + image_size, 4);
  PutLe(bmp, 0, 4);
  PutLe(bmp, 54, 4);
  PutLe(bmp, 40, 4);
  PutLe(bmp, static_cast<uint32_t>(w), 4);
  PutLe(bmp, static_cast<uint32_t>(h), 4);  // Bottom-up.
  PutLe(bmp, 1, 2);
  PutLe(bmp, 24, 2);
  PutLe(bmp, 0, 4);
  PutLe(bmp, image_size, 4);
  PutLe(bmp, 2835, 4);
  PutLe(bmp, 2835, 4);
  PutLe(bmp, 0, 4);
  PutLe(bmp, 0, 4);

  for (int y = h - 1; y >= 0; --y) {
    const unsigned char* row = preview_.data() + static_cast<size_t>(y) * w * 4;
    for (int x = 0; x < w; ++x) {
      const unsigned char* p = row + x * 4;
      if (checker) {
        // Premultiplied BGRA over a grey checkerboard.
        const int bg = (((x / 8) + (y / 8)) % 2) ? 0x99 : 0x66;
        const int inv = 255 - p[3];
        for (int c = 0; c < 3; ++c) {
          bmp.push_back(static_cast<char>(p[c] + bg * inv / 255));
        }
      } else {
        bmp.push_back(static_cast<char>(p[0]));
        bmp.push_back(static_cast<char>(p[1]));
        bmp.push_back(static_cast<char>(p[2]));
      }
    }
    for (int pad = w * 3; pad < row_size; ++pad) {
      bmp.push_back(0);
    }
  }
  return bmp;
}
