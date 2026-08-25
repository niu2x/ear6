#import <AppKit/AppKit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreGraphics/CoreGraphics.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "desktop_session.h"
#include "save_store.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using ear6::desktop::AudioRingBuffer;
using ear6::desktop::DesktopSession;
using ear6::desktop::SaveEntry;
using ear6::desktop::SaveStore;

namespace {

constexpr double EMULATION_INTERVAL_SECONDS = 1.0 / 60.0;
constexpr double AUDIO_SAMPLE_RATE = 96000.0;
constexpr UInt32 AUDIO_CHANNEL_COUNT = 2;
constexpr UInt32 AUDIO_FRAMES_PER_BUFFER = 2048;
constexpr UInt32 AUDIO_BUFFER_COUNT = 3;

NSString* to_ns_string(const std::string& value) {
    NSString* result = [[NSString alloc] initWithBytes:value.data()
                                               length:value.size()
                                             encoding:NSUTF8StringEncoding];
    return result ?: @"";
}

std::string to_path_string(NSString* value) {
    const char* path = [value fileSystemRepresentation];
    return path ? std::string(path) : std::string();
}

NSImage* image_from_rgba(const uint8_t* rgba, size_t size, int width, int height) {
    if (!rgba || size == 0 || width <= 0 || height <= 0) return nil;

    NSData* data = [NSData dataWithBytes:rgba length:size];
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,
        32,
        static_cast<size_t>(width) * 4,
        color_space,
        static_cast<CGBitmapInfo>(kCGImageAlphaLast) | kCGBitmapByteOrder32Big,
        provider,
        nullptr,
        false,
        kCGRenderingIntentDefault
    );
    NSImage* result = image
        ? [[NSImage alloc] initWithCGImage:image size:NSMakeSize(width, height)]
        : nil;
    if (image) CGImageRelease(image);
    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
    return result;
}

NSString* audio_error_string(OSStatus status) {
    UInt32 value = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    char text[5] = {};
    std::memcpy(text, &value, 4);
    bool printable = true;
    for (size_t index = 0; index < 4; ++index) {
        printable = printable && text[index] >= 32 && text[index] <= 126;
    }
    return printable
        ? [NSString stringWithFormat:@"Audio error '%s'", text]
        : [NSString stringWithFormat:@"Audio error %d", static_cast<int>(status)];
}

class AudioOutput {
public:
    explicit AudioOutput(AudioRingBuffer* ring_buffer)
        : ring_buffer_(ring_buffer) {}

    ~AudioOutput() {
        if (queue_) {
            AudioQueueStop(queue_, true);
            AudioQueueDispose(queue_, true);
        }
    }

    bool start(NSString** error) {
        AudioStreamBasicDescription format = {};
        format.mSampleRate = AUDIO_SAMPLE_RATE;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger
            | kLinearPCMFormatFlagIsPacked;
        format.mBytesPerPacket = AUDIO_CHANNEL_COUNT * sizeof(int16_t);
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = AUDIO_CHANNEL_COUNT * sizeof(int16_t);
        format.mChannelsPerFrame = AUDIO_CHANNEL_COUNT;
        format.mBitsPerChannel = 16;

        OSStatus status = AudioQueueNewOutput(
            &format,
            output_callback,
            this,
            nullptr,
            nullptr,
            0,
            &queue_
        );
        if (status != noErr) {
            if (error) *error = audio_error_string(status);
            return false;
        }

        for (UInt32 index = 0; index < AUDIO_BUFFER_COUNT; ++index) {
            AudioQueueBufferRef buffer = nullptr;
            status = AudioQueueAllocateBuffer(
                queue_,
                AUDIO_FRAMES_PER_BUFFER * AUDIO_CHANNEL_COUNT * sizeof(int16_t),
                &buffer
            );
            if (status != noErr) {
                if (error) *error = audio_error_string(status);
                return false;
            }
            fill_and_enqueue(buffer);
        }

        status = AudioQueueStart(queue_, nullptr);
        if (status != noErr) {
            if (error) *error = audio_error_string(status);
            return false;
        }
        return true;
    }

private:
    static void output_callback(
        void* user_data,
        AudioQueueRef,
        AudioQueueBufferRef buffer
    ) {
        static_cast<AudioOutput*>(user_data)->fill_and_enqueue(buffer);
    }

    void fill_and_enqueue(AudioQueueBufferRef buffer) {
        const size_t sample_count = AUDIO_FRAMES_PER_BUFFER * AUDIO_CHANNEL_COUNT;
        auto* output = static_cast<int16_t*>(buffer->mAudioData);
        const size_t copied = ring_buffer_ ? ring_buffer_->pop(output, sample_count) : 0;
        std::fill(output + copied, output + sample_count, 0);
        buffer->mAudioDataByteSize = static_cast<UInt32>(sample_count * sizeof(int16_t));
        AudioQueueEnqueueBuffer(queue_, buffer, 0, nullptr);
    }

    AudioRingBuffer* ring_buffer_ = nullptr;
    AudioQueueRef queue_ = nullptr;
};

bool map_key(unsigned short key_code, Ear6NesButton* button) {
    if (!button) return false;
    switch (key_code) {
        case 7: *button = EAR6_NES_BUTTON_A; return true;       // X
        case 6: *button = EAR6_NES_BUTTON_B; return true;       // Z
        case 36:
        case 76: *button = EAR6_NES_BUTTON_START; return true;  // Return
        case 123: *button = EAR6_NES_BUTTON_LEFT; return true;
        case 124: *button = EAR6_NES_BUTTON_RIGHT; return true;
        case 125: *button = EAR6_NES_BUTTON_DOWN; return true;
        case 126: *button = EAR6_NES_BUTTON_UP; return true;
        default: return false;
    }
}

} // namespace

@protocol EmulatorInputDelegate <NSObject>
- (void)setNesButton:(Ear6NesButton)button pressed:(BOOL)pressed;
- (void)clearEmulatorInput;
@end

@interface EmulatorView : NSView
@property(nonatomic, weak) id<EmulatorInputDelegate> inputDelegate;
- (void)setFrameData:(const std::vector<uint8_t>&)data width:(int)width height:(int)height;
@end

@implementation EmulatorView {
    NSImage* _frameImage;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)setFrameData:(const std::vector<uint8_t>&)data width:(int)width height:(int)height {
    _frameImage = image_from_rgba(data.data(), data.size(), width, height);
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    [[NSColor blackColor] setFill];
    NSRectFill(self.bounds);
    if (!_frameImage) return;

    const NSSize image_size = _frameImage.size;
    const CGFloat scale = std::min(
        self.bounds.size.width / image_size.width,
        self.bounds.size.height / image_size.height
    );
    const NSSize target_size = NSMakeSize(image_size.width * scale, image_size.height * scale);
    const NSRect target = NSMakeRect(
        (self.bounds.size.width - target_size.width) / 2.0,
        (self.bounds.size.height - target_size.height) / 2.0,
        target_size.width,
        target_size.height
    );
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    context.imageInterpolation = NSImageInterpolationNone;
    [_frameImage drawInRect:target
                   fromRect:NSMakeRect(0, 0, image_size.width, image_size.height)
                  operation:NSCompositingOperationCopy
                   fraction:1.0
             respectFlipped:YES
                      hints:nil];
}

- (void)keyDown:(NSEvent*)event {
    if (event.isARepeat) return;
    Ear6NesButton button;
    if (map_key(event.keyCode, &button)) {
        [self.inputDelegate setNesButton:button pressed:YES];
        return;
    }
    [super keyDown:event];
}

- (void)keyUp:(NSEvent*)event {
    if (event.isARepeat) return;
    Ear6NesButton button;
    if (map_key(event.keyCode, &button)) {
        [self.inputDelegate setNesButton:button pressed:NO];
        return;
    }
    [super keyUp:event];
}

- (void)flagsChanged:(NSEvent*)event {
    if (event.keyCode == 56 || event.keyCode == 60) {
        const BOOL pressed = (event.modifierFlags & NSEventModifierFlagShift) != 0;
        [self.inputDelegate setNesButton:EAR6_NES_BUTTON_SELECT pressed:pressed];
        return;
    }
    [super flagsChanged:event];
}

@end

@interface AppDelegate : NSObject <
    NSApplicationDelegate,
    NSMenuDelegate,
    NSMenuItemValidation,
    NSWindowDelegate,
    EmulatorInputDelegate
>
@end

@implementation AppDelegate {
    NSWindow* _window;
    EmulatorView* _emulatorView;
    NSTimer* _timer;
    NSMenu* _loadStateMenu;
    NSMenuItem* _runPauseItem;
    NSMenuItem* _saveStateItem;
    NSMenuItem* _resetItem;
    NSMutableArray<NSString*>* _pendingOpenFiles;
    std::unique_ptr<DesktopSession> _session;
    std::unique_ptr<SaveStore> _saveStore;
    std::unique_ptr<AudioOutput> _audioOutput;
    bool _running;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _pendingOpenFiles = [[NSMutableArray alloc] init];
        _session = std::make_unique<DesktopSession>();

        NSString* application_support = NSSearchPathForDirectoriesInDomains(
            NSApplicationSupportDirectory,
            NSUserDomainMask,
            YES
        ).firstObject;
        NSString* state_directory = [[application_support stringByAppendingPathComponent:@"Ear6"]
            stringByAppendingPathComponent:@"states"];
        _saveStore = std::make_unique<SaveStore>(to_path_string(state_directory));
        _audioOutput = std::make_unique<AudioOutput>(&_session->get_audio_buffer());
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [self buildMenus];
    [self buildWindow];

    NSString* audio_error = nil;
    if (!_audioOutput->start(&audio_error)) {
        [self showError:@"Audio output is unavailable" detail:audio_error];
    }

    NSString* initial_path = _pendingOpenFiles.firstObject;
    if (!initial_path) {
        for (NSString* argument in NSProcessInfo.processInfo.arguments) {
            if ([argument isEqualToString:NSProcessInfo.processInfo.arguments.firstObject]
                || [argument hasPrefix:@"-"]) {
                continue;
            }
            initial_path = argument;
            break;
        }
    }

    if (initial_path) {
        [self loadPath:initial_path];
    } else {
        std::string error;
        if (!_session->load_test(&error)) {
            [self showError:@"Unable to start Ear6" detail:to_ns_string(error)];
        } else {
            _running = true;
            [self updateFrame];
        }
    }

    _timer = [NSTimer timerWithTimeInterval:EMULATION_INTERVAL_SECONDS
                                     target:self
                                   selector:@selector(stepEmulation:)
                                   userInfo:nil
                                    repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_emulatorView];
    [NSApp activateIgnoringOtherApps:YES];
    [self updateActions];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    [_timer invalidate];
    _session->clear_input();
    _audioOutput.reset();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)application:(NSApplication*)sender openFiles:(NSArray<NSString*>*)filenames {
    if (!_window) {
        [_pendingOpenFiles addObjectsFromArray:filenames];
        [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
        return;
    }
    BOOL loaded = filenames.count > 0 && [self loadPath:filenames.firstObject];
    [sender replyToOpenOrPrint:loaded
        ? NSApplicationDelegateReplySuccess
        : NSApplicationDelegateReplyFailure];
}

- (void)windowDidResignKey:(NSNotification*)notification {
    (void)notification;
    [self clearEmulatorInput];
}

- (void)buildWindow {
    const NSRect frame = NSMakeRect(0, 0, 768, 720);
    _window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:NSWindowStyleMaskTitled
                                                   | NSWindowStyleMaskClosable
                                                   | NSWindowStyleMaskMiniaturizable
                                                   | NSWindowStyleMaskResizable
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    _window.delegate = self;
    _window.title = @"Ear6";
    _window.minSize = NSMakeSize(256, 240);
    _window.backgroundColor = [NSColor blackColor];
    [_window center];

    _emulatorView = [[EmulatorView alloc] initWithFrame:frame];
    _emulatorView.inputDelegate = self;
    _window.contentView = _emulatorView;
}

- (void)buildMenus {
    NSMenu* main_menu = [[NSMenu alloc] initWithTitle:@"Main Menu"];
    NSApp.mainMenu = main_menu;

    NSMenuItem* application_item = [[NSMenuItem alloc] init];
    [main_menu addItem:application_item];
    NSMenu* application_menu = [[NSMenu alloc] initWithTitle:@"Ear6"];
    application_item.submenu = application_menu;
    [application_menu addItemWithTitle:@"About Ear6"
                                action:@selector(orderFrontStandardAboutPanel:)
                         keyEquivalent:@""];
    [application_menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* quit_item = [[NSMenuItem alloc]
        initWithTitle:@"Quit Ear6"
               action:@selector(terminate:)
        keyEquivalent:@"q"];
    quit_item.target = NSApp;
    [application_menu addItem:quit_item];

    NSMenuItem* file_item = [[NSMenuItem alloc] init];
    [main_menu addItem:file_item];
    NSMenu* file_menu = [[NSMenu alloc] initWithTitle:@"File"];
    file_item.submenu = file_menu;
    [file_menu addItemWithTitle:@"Open..." action:@selector(openContent:) keyEquivalent:@"o"].target = self;
    _saveStateItem = [file_menu addItemWithTitle:@"Save State"
                                          action:@selector(saveState:)
                                   keyEquivalent:@"s"];
    _saveStateItem.target = self;
    _loadStateMenu = [[NSMenu alloc] initWithTitle:@"Load State"];
    _loadStateMenu.delegate = self;
    NSMenuItem* load_item = [[NSMenuItem alloc] initWithTitle:@"Load State"
                                                      action:nil
                                               keyEquivalent:@""];
    load_item.submenu = _loadStateMenu;
    [file_menu addItem:load_item];

    NSMenuItem* emulation_item = [[NSMenuItem alloc] init];
    [main_menu addItem:emulation_item];
    NSMenu* emulation_menu = [[NSMenu alloc] initWithTitle:@"Emulation"];
    emulation_item.submenu = emulation_menu;
    _runPauseItem = [emulation_menu addItemWithTitle:@"Pause"
                                              action:@selector(toggleRunPause:)
                                       keyEquivalent:@" "];
    _runPauseItem.target = self;
    _resetItem = [emulation_menu addItemWithTitle:@"Reset"
                                           action:@selector(resetEmulation:)
                                    keyEquivalent:@"r"];
    _resetItem.target = self;

    NSMenuItem* window_item = [[NSMenuItem alloc] init];
    [main_menu addItem:window_item];
    NSMenu* window_menu = [[NSMenu alloc] initWithTitle:@"Window"];
    window_item.submenu = window_menu;
    [window_menu addItemWithTitle:@"Minimize"
                           action:@selector(performMiniaturize:)
                    keyEquivalent:@"m"];
    NSApp.windowsMenu = window_menu;
}

- (void)openContent:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    UTType* nes_type = [UTType typeWithFilenameExtension:@"nes"];
    UTType* state_type = [UTType typeWithFilenameExtension:@"e6s"];
    panel.allowedContentTypes = @[nes_type ?: UTTypeData, state_type ?: UTTypeData];
    if ([panel runModal] == NSModalResponseOK) [self loadPath:panel.URL.path];
}

- (BOOL)loadPath:(NSString*)path {
    if ([[path.pathExtension lowercaseString] isEqualToString:@"e6s"]) {
        SaveEntry entry;
        entry.path = to_path_string(path);
        std::vector<uint8_t> state;
        std::ifstream input(entry.path, std::ios::binary | std::ios::ate);
        if (!input) {
            [self showError:@"Unable to open state" detail:path];
            return NO;
        }
        const std::streamsize size = input.tellg();
        if (size < 0) {
            [self showError:@"Unable to read state" detail:path];
            return NO;
        }
        state.resize(static_cast<size_t>(size));
        input.seekg(0);
        if (size > 0 && !input.read(reinterpret_cast<char*>(state.data()), size)) {
            [self showError:@"Unable to read state" detail:path];
            return NO;
        }
        return [self restoreState:state];
    }

    std::string error;
    const Ear6SystemType system = [[path.pathExtension lowercaseString] isEqualToString:@"nes"]
        ? EAR6_SYSTEM_NES
        : EAR6_SYSTEM_TEST;
    if (!_session->load_file(system, to_path_string(path), &error)) {
        [self showError:@"Unable to load content" detail:to_ns_string(error)];
        return NO;
    }
    _running = true;
    [self updateFrame];
    [self updateActions];
    [_window makeFirstResponder:_emulatorView];
    return YES;
}

- (void)saveState:(id)sender {
    (void)sender;
    std::vector<uint8_t> state;
    std::string error;
    if (!_session->save_state(&state, &error)) {
        [self showError:@"Unable to save state" detail:to_ns_string(error)];
        return;
    }
    std::filesystem::path path;
    if (!_saveStore->save(state, &path, &error)) {
        [self showError:@"Unable to save state" detail:to_ns_string(error)];
        return;
    }
}

- (void)loadStateItem:(NSMenuItem*)sender {
    const std::filesystem::path selected_path = to_path_string(sender.representedObject);
    std::string error;
    const std::vector<SaveEntry> entries = _saveStore->list(&error);
    auto selected = std::find_if(entries.begin(), entries.end(), [&](const SaveEntry& entry) {
        return entry.path == selected_path;
    });
    if (selected == entries.end()) {
        [self showError:@"Unable to load state" detail:@"The save is no longer available."];
        return;
    }

    std::vector<uint8_t> state;
    if (!_saveStore->load(*selected, &state, &error)) {
        [self showError:@"Unable to load state" detail:to_ns_string(error)];
        return;
    }
    [self restoreState:state];
}

- (BOOL)restoreState:(const std::vector<uint8_t>&)state {
    std::string error;
    if (!_session->load_state(state, &error)) {
        [self showError:@"Unable to load state" detail:to_ns_string(error)];
        return NO;
    }
    _running = false;
    [self updateFrame];
    [self updateActions];
    [_window makeFirstResponder:_emulatorView];
    return YES;
}

- (void)toggleRunPause:(id)sender {
    (void)sender;
    if (!_session->has_content()) return;
    _running = !_running;
    if (!_running) {
        [self clearEmulatorInput];
        _session->clear_audio();
    }
    [self updateActions];
}

- (void)resetEmulation:(id)sender {
    (void)sender;
    std::string error;
    if (!_session->reset(&error)) {
        [self showError:@"Unable to reset" detail:to_ns_string(error)];
        return;
    }
    _running = false;
    [self updateFrame];
    [self updateActions];
}

- (void)stepEmulation:(NSTimer*)timer {
    (void)timer;
    if (!_running || !_session->has_content()) return;
    std::string error;
    if (!_session->step(&error)) {
        _running = false;
        [self updateActions];
        [self showError:@"Emulation stopped" detail:to_ns_string(error)];
        return;
    }
    [self updateFrame];
}

- (void)updateFrame {
    [_emulatorView setFrameData:_session->get_frame()
                          width:_session->get_frame_width()
                         height:_session->get_frame_height()];
    NSString* name = to_ns_string(_session->get_display_name());
    _window.title = name.length > 0
        ? [NSString stringWithFormat:@"Ear6 - %@", name]
        : @"Ear6";
}

- (void)updateActions {
    const BOOL has_content = _session->has_content();
    _saveStateItem.enabled = has_content;
    _runPauseItem.enabled = has_content;
    _resetItem.enabled = _session->can_reset();
    _runPauseItem.title = _running ? @"Pause" : @"Run";
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
    if (menuItem == _saveStateItem || menuItem == _runPauseItem) {
        return _session->has_content();
    }
    if (menuItem == _resetItem) return _session->can_reset();
    return YES;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
    if (menu != _loadStateMenu) return;
    [menu removeAllItems];

    std::string error;
    const std::vector<SaveEntry> entries = _saveStore->list(&error);
    if (entries.empty()) {
        NSMenuItem* empty_item = [[NSMenuItem alloc]
            initWithTitle:error.empty() ? @"No Saved States" : to_ns_string(error)
                   action:nil
            keyEquivalent:@""];
        empty_item.enabled = NO;
        [menu addItem:empty_item];
        return;
    }

    NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
    formatter.dateStyle = NSDateFormatterShortStyle;
    formatter.timeStyle = NSDateFormatterShortStyle;
    for (const SaveEntry& entry : entries) {
        const auto seconds = std::chrono::duration<double>(
            entry.saved_at.time_since_epoch()
        ).count();
        NSDate* date = [NSDate dateWithTimeIntervalSince1970:seconds];
        NSString* title = [NSString stringWithFormat:@"%@ - %@",
            to_ns_string(entry.display_name),
            [formatter stringFromDate:date]];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                     action:@selector(loadStateItem:)
                                              keyEquivalent:@""];
        item.target = self;
        item.representedObject = to_ns_string(entry.path.string());
        if (!entry.preview.empty()) {
            NSImage* preview = image_from_rgba(
                entry.preview.data(),
                entry.preview.size(),
                entry.preview_width,
                entry.preview_height
            );
            preview.size = NSMakeSize(48, 45);
            item.image = preview;
        }
        [menu addItem:item];
    }
}

- (void)setNesButton:(Ear6NesButton)button pressed:(BOOL)pressed {
    _session->set_nes_button(button, pressed == YES);
}

- (void)clearEmulatorInput {
    _session->clear_input();
}

- (void)showError:(NSString*)message detail:(NSString*)detail {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = message ?: @"Ear6 Error";
    alert.informativeText = detail ?: @"";
    [alert runModal];
}

@end

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        application.activationPolicy = NSApplicationActivationPolicyRegular;
        AppDelegate* delegate = [[AppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
