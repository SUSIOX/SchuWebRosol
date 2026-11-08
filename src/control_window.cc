#include "control_window.h"

#include "control_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace {

class ControlBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  ControlBrowserViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(ControlBrowserViewDelegate);
};

class ControlWindowDelegate : public CefWindowDelegate {
 public:
  ControlWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                        const std::string& title)
      : browser_view_(browser_view), title_(title) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->SetTitle(title_);
    window->Show();
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    return browser ? browser->GetHost()->TryCloseBrowser() : true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(1000, 860);
  }

  CefSize GetMinimumSize(CefRefPtr<CefView> view) override {
    return CefSize(560, 480);
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  std::string title_;

  IMPLEMENT_REFCOUNTING(ControlWindowDelegate);
};

}  // namespace

void ShowControlWindow(const std::string& title) {
  CEF_REQUIRE_UI_THREAD();
  CefBrowserSettings browser_settings;
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      new ControlClient(), kControlOrigin, browser_settings, nullptr, nullptr,
      new ControlBrowserViewDelegate());
  CefWindow::CreateTopLevelWindow(
      new ControlWindowDelegate(browser_view, title));
}
