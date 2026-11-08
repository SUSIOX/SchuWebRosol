// Publishes BGRA frames to other applications (Spout on Windows, Syphon on
// macOS). All methods are called on the CEF UI thread.
#ifndef SCHUWEBROSOL_FRAME_SENDER_H_
#define SCHUWEBROSOL_FRAME_SENDER_H_

#include <memory>
#include <string>

class FrameSender {
 public:
  virtual ~FrameSender() = default;

  virtual void SetName(const std::string& name) = 0;
  // |bgra| is top-down, tightly packed (width * 4 bytes per row).
  virtual bool Send(const void* bgra, int width, int height) = 0;
  // Human readable backend name, e.g. "Spout".
  virtual std::string Backend() const = 0;
  // Number of connected receivers, or -1 when the backend can't tell.
  virtual int ClientCount() const { return -1; }

  static std::unique_ptr<FrameSender> Create(const std::string& name);
};

#endif  // SCHUWEBROSOL_FRAME_SENDER_H_
