#include "frame_sender.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Syphon/Syphon.h>

namespace {

class SyphonFrameSender : public FrameSender {
 public:
  explicit SyphonFrameSender(const std::string& name) : name_(name) {
    device_ = MTLCreateSystemDefaultDevice();
    queue_ = [device_ newCommandQueue];
    server_ = [[SyphonMetalServer alloc] initWithName:NSName() device:device_ options:nil];
  }

  ~SyphonFrameSender() override {
    [server_ stop];
    server_ = nil;
  }

  void SetName(const std::string& name) override {
    name_ = name;
    server_.name = NSName();
  }

  bool Send(const void* bgra, int width, int height) override {
    if (!server_ || !device_) {
      return false;
    }
    @autoreleasepool {
      if (!texture_ || texture_.width != static_cast<NSUInteger>(width) ||
          texture_.height != static_cast<NSUInteger>(height)) {
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                         width:width
                                        height:height
                                     mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        // Apple GPUs support shared textures; Intel and discrete GPUs need
        // managed storage for CPU-written textures.
        desc.storageMode = [device_ supportsFamily:MTLGPUFamilyApple1]
                               ? MTLStorageModeShared
                               : MTLStorageModeManaged;
        texture_ = [device_ newTextureWithDescriptor:desc];
        if (!texture_) {
          return false;
        }
      }
      [texture_ replaceRegion:MTLRegionMake2D(0, 0, width, height)
                  mipmapLevel:0
                    withBytes:bgra
                  bytesPerRow:static_cast<NSUInteger>(width) * 4];
      id<MTLCommandBuffer> command_buffer = [queue_ commandBuffer];
      [server_ publishFrameTexture:texture_
                   onCommandBuffer:command_buffer
                       imageRegion:NSMakeRect(0, 0, width, height)
                           flipped:NO];
      [command_buffer commit];
      // The texture is overwritten by the next frame, so wait for the copy.
      [command_buffer waitUntilCompleted];
    }
    return true;
  }

  std::string Backend() const override { return "Syphon"; }

  int ClientCount() const override { return server_.hasClients ? 1 : 0; }

 private:
  NSString* NSName() const { return [NSString stringWithUTF8String:name_.c_str()]; }

  std::string name_;
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  id<MTLTexture> texture_ = nil;
  SyphonMetalServer* server_ = nil;
};

}  // namespace

std::unique_ptr<FrameSender> FrameSender::Create(const std::string& name) {
  return std::make_unique<SyphonFrameSender>(name);
}
