#include "osr_client.h"

#include "controller.h"
#include "include/wrapper/cef_helpers.h"

OsrClient::OsrClient(int width, int height) : width_(width), height_(height) {}

void OsrClient::SetSize(int width, int height) {
  CEF_REQUIRE_UI_THREAD();
  if (width == width_ && height == height_) {
    return;
  }
  width_ = width;
  height_ = height;
  if (browser_) {
    browser_->GetHost()->WasResized();
  }
}

void OsrClient::Close() {
  CEF_REQUIRE_UI_THREAD();
  if (browser_) {
    browser_->GetHost()->CloseBrowser(true);
  }
}

void OsrClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                const CefString& url) {
  if (frame->IsMain()) {
    Controller::Get()->OnOutputAddressChange(this, url.ToString());
  }
}

bool OsrClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              int popup_id,
                              const CefString& target_url,
                              const CefString& target_frame_name,
                              WindowOpenDisposition target_disposition,
                              bool user_gesture,
                              const CefPopupFeatures& popupFeatures,
                              CefWindowInfo& windowInfo,
                              CefRefPtr<CefClient>& client,
                              CefBrowserSettings& settings,
                              CefRefPtr<CefDictionaryValue>& extra_info,
                              bool* no_javascript_access) {
  // There is nobody to look at a popup; open it in the output instead.
  if (!target_url.empty()) {
    browser->GetMainFrame()->LoadURL(target_url);
  }
  return true;
}

void OsrClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  Controller::Get()->OnBrowserCreated();
  Controller::Get()->OnOutputCreated(this);
}

void OsrClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = nullptr;
  Controller::Get()->OnBrowserClosed();
}

void OsrClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                     bool isLoading,
                                     bool canGoBack,
                                     bool canGoForward) {
  Controller::Get()->OnOutputLoadingState(this, isLoading);
}

void OsrClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int httpStatusCode) {
  if (frame->IsMain()) {
    Controller::Get()->OnOutputLoadEnd(this, frame, httpStatusCode);
  }
}

void OsrClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            ErrorCode errorCode,
                            const CefString& errorText,
                            const CefString& failedUrl) {
  if (frame->IsMain() && errorCode != ERR_ABORTED) {
    Controller::Get()->OnOutputLoadError(
        this, errorText.ToString() + " (" + failedUrl.ToString() + ")");
  }
}

void OsrClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
  rect = CefRect(0, 0, width_, height_);
}

bool OsrClient::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                              CefScreenInfo& screen_info) {
  screen_info.device_scale_factor = 1.0f;
  screen_info.rect = CefRect(0, 0, width_, height_);
  screen_info.available_rect = screen_info.rect;
  return true;
}

void OsrClient::OnPaint(CefRefPtr<CefBrowser> browser,
                        PaintElementType type,
                        const RectList& dirtyRects,
                        const void* buffer,
                        int width,
                        int height) {
  if (type == PET_VIEW) {
    Controller::Get()->OnFrame(this, buffer, width, height);
  }
}
