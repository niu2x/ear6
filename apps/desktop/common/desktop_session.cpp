#include "desktop_session.h"

#include <filesystem>
#include <limits>
#include <utility>

namespace ear6::desktop {
namespace {

constexpr size_t AUDIO_BUFFER_SAMPLES = 96000 * 2;

void set_error(std::string* error, const char* message) {
    if (error) *error = message;
}

} // namespace

DesktopSession::DesktopSession()
    : audio_buffer_(AUDIO_BUFFER_SAMPLES) {}

DesktopSession::~DesktopSession() {
    ear6_destroy(context_);
}

bool DesktopSession::load_test(std::string* error) {
    Ear6* candidate = ear6_create(EAR6_SYSTEM_TEST);
    if (!candidate) {
        set_error(error, "Unable to create the Test system");
        return false;
    }
    if (ear6_load_from_memory(candidate, nullptr, 0, nullptr) != 0) {
        ear6_destroy(candidate);
        set_error(error, "Unable to initialize the Test system");
        return false;
    }
    return replace_context(
        candidate,
        EAR6_SYSTEM_TEST,
        SourceType::TEST,
        {},
        "Test System",
        error
    );
}

bool DesktopSession::load_file(
    Ear6SystemType system,
    const std::string& path,
    std::string* error
) {
    Ear6* candidate = ear6_create(system);
    if (!candidate) {
        set_error(error, "The selected system is not available");
        return false;
    }
    if (ear6_load_from_file(candidate, path.c_str()) != 0) {
        ear6_destroy(candidate);
        set_error(error, "Ear6 could not load this file");
        return false;
    }
    std::string name = std::filesystem::path(path).filename().string();
    return replace_context(
        candidate,
        system,
        SourceType::FILE,
        path,
        std::move(name),
        error
    );
}

bool DesktopSession::load_state(const std::vector<uint8_t>& state, std::string* error) {
    Ear6StateInfo info = {};
    if (state.empty() || ear6_get_state_info(state.data(), state.size(), &info) != 0) {
        set_error(error, "Invalid or unsupported Ear6 state");
        return false;
    }

    Ear6* candidate = ear6_create(info.system_type);
    if (!candidate) {
        set_error(error, "The state targets an unavailable system");
        return false;
    }
    if (ear6_load_state_from_memory(candidate, state.data(), state.size()) != 0) {
        ear6_destroy(candidate);
        set_error(error, "Ear6 could not restore this state");
        return false;
    }

    std::string name = "Saved State";
    if (info.content_name_hint && info.content_name_hint_size > 0) {
        name.assign(info.content_name_hint, info.content_name_hint_size);
    }
    return replace_context(
        candidate,
        info.system_type,
        SourceType::STATE,
        {},
        std::move(name),
        error
    );
}

bool DesktopSession::reset(std::string* error) {
    if (source_ == SourceType::TEST) return load_test(error);
    if (source_ == SourceType::FILE) return load_file(system_, source_path_, error);
    set_error(error, "This session cannot be reset without its original file");
    return false;
}

bool DesktopSession::step(std::string* error) {
    if (!context_) {
        set_error(error, "No content is loaded");
        return false;
    }
    if (ear6_step(context_) != 0) {
        set_error(error, "Emulation step failed");
        return false;
    }
    copy_frame();
    return true;
}

bool DesktopSession::save_state(std::vector<uint8_t>* state, std::string* error) const {
    if (!context_ || !state) {
        set_error(error, "No content is loaded");
        return false;
    }

    size_t size = 0;
    if (ear6_save_state_to_memory(context_, nullptr, 0, &size) != 0) {
        set_error(error, "Unable to determine state size");
        return false;
    }
    try {
        state->resize(size);
    } catch (...) {
        set_error(error, "Unable to allocate the state buffer");
        return false;
    }
    if (ear6_save_state_to_memory(context_, state->data(), state->size(), &size) != 0) {
        state->clear();
        set_error(error, "Unable to serialize state");
        return false;
    }
    state->resize(size);
    return true;
}

int DesktopSession::set_nes_button(Ear6NesButton button, bool pressed) {
    if (!context_ || system_ != EAR6_SYSTEM_NES) return -1;
    return ear6_nes_set_button_state(context_, button, pressed ? 1 : 0);
}

void DesktopSession::clear_input() {
    if (context_ && system_ == EAR6_SYSTEM_NES) ear6_nes_clear_input(context_);
}

void DesktopSession::clear_audio() {
    audio_buffer_.clear();
}

bool DesktopSession::has_content() const {
    return context_ != nullptr;
}

bool DesktopSession::can_reset() const {
    return source_ == SourceType::TEST || source_ == SourceType::FILE;
}

Ear6SystemType DesktopSession::get_system_type() const {
    return system_;
}

const std::string& DesktopSession::get_display_name() const {
    return display_name_;
}

const std::vector<uint8_t>& DesktopSession::get_frame() const {
    return frame_;
}

int DesktopSession::get_frame_width() const {
    return frame_width_;
}

int DesktopSession::get_frame_height() const {
    return frame_height_;
}

AudioRingBuffer& DesktopSession::get_audio_buffer() {
    return audio_buffer_;
}

void DesktopSession::audio_callback(const int16_t* data, int num_samples, void* user_data) {
    if (!user_data || !data || num_samples <= 0) return;
    auto* session = static_cast<DesktopSession*>(user_data);
    session->audio_buffer_.push(data, static_cast<size_t>(num_samples) * 2);
}

bool DesktopSession::replace_context(
    Ear6* candidate,
    Ear6SystemType system,
    SourceType source,
    std::string source_path,
    std::string display_name,
    std::string* error
) {
    if (!candidate) {
        set_error(error, "Unable to create an emulation context");
        return false;
    }

    ear6_set_audio_callback(candidate, audio_callback, this);
    ear6_destroy(context_);
    context_ = candidate;
    system_ = system;
    source_ = source;
    source_path_ = std::move(source_path);
    display_name_ = std::move(display_name);
    audio_buffer_.clear();
    copy_frame();
    if (error) error->clear();
    return true;
}

void DesktopSession::copy_frame() {
    const uint8_t* data = ear6_get_framebuffer(context_);
    const int width = ear6_get_frame_width(context_);
    const int height = ear6_get_frame_height(context_);
    if (!data || width <= 0 || height <= 0
        || static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / 4
        || static_cast<size_t>(height)
            > std::numeric_limits<size_t>::max() / (static_cast<size_t>(width) * 4)) {
        frame_.clear();
        frame_width_ = 0;
        frame_height_ = 0;
        return;
    }

    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    frame_.assign(data, data + size);
    frame_width_ = width;
    frame_height_ = height;
}

} // namespace ear6::desktop
