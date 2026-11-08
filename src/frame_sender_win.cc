#include "frame_sender.h"

// Spout redefines WIN32_LEAN_AND_MEAN, which CEF already sets globally.
#pragma warning(push)
#pragma warning(disable : 4005)
#include "SpoutDX.h"
#pragma warning(pop)

namespace {

class SpoutFrameSender : public FrameSender {
 public:
  explicit SpoutFrameSender(const std::string& name) : name_(name) {}

  ~SpoutFrameSender() override {
    if (ready_) {
      spout_.ReleaseSender();
      spout_.CloseDirectX11();
    }
  }

  void SetName(const std::string& name) override {
    if (name == name_) {
      return;
    }
    name_ = name;
    if (ready_) {
      spout_.ReleaseSender();
      spout_.SetSenderName(name_.c_str());
    }
  }

  bool Send(const void* bgra, int width, int height) override {
    if (!initialized_) {
      initialized_ = true;
      ready_ = spout_.OpenDirectX11();
      if (ready_) {
        spout_.SetSenderName(name_.c_str());
        spout_.SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
      }
    }
    if (!ready_) {
      return false;
    }
    return spout_.SendImage(static_cast<const unsigned char*>(bgra),
                            static_cast<unsigned int>(width),
                            static_cast<unsigned int>(height));
  }

  std::string Backend() const override { return "Spout"; }

 private:
  spoutDX spout_;
  std::string name_;
  bool initialized_ = false;
  bool ready_ = false;
};

}  // namespace

std::unique_ptr<FrameSender> FrameSender::Create(const std::string& name) {
  return std::make_unique<SpoutFrameSender>(name);
}
