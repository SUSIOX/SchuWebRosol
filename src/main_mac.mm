#import <Cocoa/Cocoa.h>

#include "app.h"
#include "controller.h"
#include "include/cef_application_mac.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_library_loader.h"

@interface SchuAppDelegate : NSObject <NSApplicationDelegate>
- (void)createMenu:(id)object;
@end

@interface SchuApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation SchuApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

- (void)terminate:(id)sender {
  // Close the browsers first; the message loop quits after they are gone.
  Controller::Get()->CloseAll();
}
@end

@implementation SchuAppDelegate
- (void)createMenu:(id)object {
  NSMenu* menubar = [[NSMenu alloc] init];
  NSMenuItem* app_item = [[NSMenuItem alloc] init];
  [menubar addItem:app_item];
  NSMenu* app_menu = [[NSMenu alloc] init];
  [app_menu addItemWithTitle:@"Quit SchuWebRosol"
                      action:@selector(terminate:)
               keyEquivalent:@"q"];
  [app_item setSubmenu:app_menu];

  NSMenuItem* edit_item = [[NSMenuItem alloc] init];
  [menubar addItem:edit_item];
  NSMenu* edit_menu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [edit_menu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
  [edit_menu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
  [edit_menu addItem:[NSMenuItem separatorItem]];
  [edit_menu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
  [edit_menu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [edit_menu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
  [edit_menu addItemWithTitle:@"Select All"
                       action:@selector(selectAll:)
                keyEquivalent:@"a"];
  [edit_item setSubmenu:edit_menu];
  [NSApp setMainMenu:menubar];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  return NSTerminateNow;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}
@end

int main(int argc, char* argv[]) {
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    return 1;
  }

  CefMainArgs main_args(argc, argv);

  @autoreleasepool {
    [SchuApplication sharedApplication];
    CHECK([NSApp isKindOfClass:[SchuApplication class]]);

    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::CreateCommandLine();
    command_line->InitFromArgv(argc, argv);
    CefSettings settings = MakeCefSettings(command_line);

    CefRefPtr<SchuApp> app(new SchuApp);
    if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
      return CefGetExitCode();
    }

    SchuAppDelegate* delegate = [[SchuAppDelegate alloc] init];
    NSApp.delegate = delegate;
    [delegate performSelectorOnMainThread:@selector(createMenu:)
                               withObject:nil
                            waitUntilDone:NO];

    CefRunMessageLoop();
    CefShutdown();
    delegate = nil;
  }
  return 0;
}
