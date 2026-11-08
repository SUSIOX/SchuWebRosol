// Linux has no Spout/Syphon; this backend exists for development and tests.
// Set SCHUWEBROSOL_DUMP=/path/frame.ppm to write the latest frame (at most
// once per second) to a PPM file.
#include "frame_sender.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace {

class DumpFrameSender : public FrameSender {
 public:
  explicit DumpFrameSender(const std::string& name) : name_(name) {
    if (const char* path = std::getenv("SCHUWEBROSOL_DUMP")) {
      path_ = path;
    }
  }

  void SetName(const std::string& name) override { name_ = name; }

  bool Send(const void* bgra, int width, int height) override {
    if (path_.empty()) {
      return true;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - last_dump_ < std::chrono::seconds(1)) {
      return true;
    }
    last_dump_ = now;
    const auto* src = static_cast<const unsigned char*>(bgra);
    std::vector<unsigned char> rgb(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0, n = static_cast<size_t>(width) * height; i < n; ++i) {
      rgb[i * 3 + 0] = src[i * 4 + 2];
      rgb[i * 3 + 1] = src[i * 4 + 1];
      rgb[i * 3 + 2] = src[i * 4 + 0];
    }
    const std::string tmp = path_ + ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out << "P6\n" << width << " " << height << "\n255\n";
      out.write(reinterpret_cast<const char*>(rgb.data()),
                static_cast<std::streamsize>(rgb.size()));
    }
    std::rename(tmp.c_str(), path_.c_str());
    return true;
  }

  std::string Backend() const override { return "None (Linux)"; }

 private:
  std::string name_;
  std::string path_;
  std::chrono::steady_clock::time_point last_dump_;
};

}  // namespace

std::unique_ptr<FrameSender> FrameSender::Create(const std::string& name) {
  return std::make_unique<DumpFrameSender>(name);
}
