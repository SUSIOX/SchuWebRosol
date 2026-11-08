#include "control_client.h"

#include <algorithm>
#include <cstring>

#include "control_html.h"
#include "controller.h"
#include "include/base/cef_callback.h"
#include "include/cef_parser.h"
#include "include/cef_resource_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

const char kControlOrigin[] = "https://schuwebrosol.local/";

namespace {

bool IsControlUrl(const std::string& url) {
  return url.rfind(kControlOrigin, 0) == 0;
}

std::string ReadBody(CefRefPtr<CefRequest> request) {
  std::string body;
  CefRefPtr<CefPostData> post_data = request->GetPostData();
  if (!post_data) {
    return body;
  }
  CefPostData::ElementVector elements;
  post_data->GetElements(elements);
  for (auto& element : elements) {
    if (element->GetType() != PDE_TYPE_BYTES) {
      continue;
    }
    const size_t size = element->GetBytesCount();
    const size_t offset = body.size();
    body.resize(offset + size);
    element->GetBytes(size, body.data() + offset);
  }
  return body;
}

CefRefPtr<CefDictionaryValue> ParseObject(const std::string& json) {
  CefRefPtr<CefValue> value = CefParseJSON(json, JSON_PARSER_RFC);
  if (value && value->GetType() == VTYPE_DICTIONARY) {
    return value->GetDictionary();
  }
  return CefDictionaryValue::Create();
}

double GetNumber(CefRefPtr<CefDictionaryValue> dict, const char* key) {
  switch (dict->GetType(key)) {
    case VTYPE_INT:
      return dict->GetInt(key);
    case VTYPE_DOUBLE:
      return dict->GetDouble(key);
    default:
      return 0.0;
  }
}

std::string ToJson(CefRefPtr<CefDictionaryValue> dict) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(dict);
  return CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
}

void PostUi(base::OnceClosure task) {
  CefPostTask(TID_UI, std::move(task));
}

class ControlResourceHandler : public CefResourceHandler {
 public:
  ControlResourceHandler() = default;

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    handle_request = true;
    CefURLParts parts;
    CefParseURL(request->GetURL(), parts);
    const std::string path = CefString(&parts.path).ToString();
    const bool post = request->GetMethod().ToString() == "POST";
    Controller* c = Controller::Get();

    if (path == "/" || path == "/index.html") {
      Respond(200, "text/html",
              std::string(reinterpret_cast<const char*>(kControlHtml),
                          kControlHtmlSize));
    } else if (path == "/api/state") {
      Respond(200, "application/json", ToJson(c->GetStatus()));
    } else if (path == "/api/preview.bmp") {
      std::string bmp = c->GetPreviewBmp();
      if (bmp.empty()) {
        Respond(204, "image/bmp", "");
      } else {
        Respond(200, "image/bmp", std::move(bmp));
      }
    } else if (post && path == "/api/settings") {
      Settings next = c->GetSettings();
      next.Merge(ParseObject(ReadBody(request)));
      PostUi(base::BindOnce(&Controller::ApplySettings, base::Unretained(c),
                            next));
      RespondOk();
    } else if (post && path == "/api/reload") {
      PostUi(base::BindOnce(&Controller::Reload, base::Unretained(c)));
      RespondOk();
    } else if (post && path == "/api/click") {
      auto args = ParseObject(ReadBody(request));
      PostUi(base::BindOnce(&Controller::Click, base::Unretained(c),
                            GetNumber(args, "x"), GetNumber(args, "y")));
      RespondOk();
    } else if (post && path == "/api/scroll") {
      auto args = ParseObject(ReadBody(request));
      PostUi(base::BindOnce(&Controller::Scroll, base::Unretained(c),
                            static_cast<int>(GetNumber(args, "dy"))));
      RespondOk();
    } else if (post && path == "/api/type") {
      auto args = ParseObject(ReadBody(request));
      PostUi(base::BindOnce(&Controller::TypeText, base::Unretained(c),
                            args->GetString("text").ToString()));
      RespondOk();
    } else if (post && path == "/api/key") {
      auto args = ParseObject(ReadBody(request));
      PostUi(base::BindOnce(&Controller::PressKey, base::Unretained(c),
                            args->GetString("key").ToString()));
      RespondOk();
    } else {
      Respond(404, "text/plain", "Not found");
    }
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    response->SetStatus(status_);
    response->SetMimeType(mime_type_);
    response->SetHeaderByName("Cache-Control", "no-store", true);
    response_length = static_cast<int64_t>(data_.size());
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    const size_t remaining = data_.size() - offset_;
    if (remaining == 0) {
      bytes_read = 0;
      return false;
    }
    const size_t n = std::min(remaining, static_cast<size_t>(bytes_to_read));
    std::memcpy(data_out, data_.data() + offset_, n);
    offset_ += n;
    bytes_read = static_cast<int>(n);
    return true;
  }

  void Cancel() override {}

 private:
  void Respond(int status, const char* mime_type, std::string data) {
    status_ = status;
    mime_type_ = mime_type;
    data_ = std::move(data);
  }
  void RespondOk() { Respond(200, "application/json", "{\"ok\":true}"); }

  int status_ = 200;
  std::string mime_type_ = "text/plain";
  std::string data_;
  size_t offset_ = 0;

  IMPLEMENT_REFCOUNTING(ControlResourceHandler);
};

}  // namespace

void ControlClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  Controller::Get()->OnBrowserCreated();
  Controller::Get()->OnControlCreated(browser);
}

void ControlClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  Controller::Get()->OnControlClosed();
  Controller::Get()->OnBrowserClosed();
}

bool ControlClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   bool user_gesture,
                                   bool is_redirect) {
  // Keep the control window on the control page (e.g. dropped links).
  return !IsControlUrl(request->GetURL().ToString());
}

CefRefPtr<CefResourceRequestHandler> ControlClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling) {
  if (IsControlUrl(request->GetURL().ToString())) {
    return this;
  }
  return nullptr;
}

CefRefPtr<CefResourceHandler> ControlClient::GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  return new ControlResourceHandler();
}
