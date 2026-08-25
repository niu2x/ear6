#pragma once

#include "audio_ring_buffer.h"

#include <ear6/ear6.h>
#include <ear6/nes.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ear6::desktop {

class DesktopSession {
public:
    DesktopSession();
    ~DesktopSession();

    DesktopSession(const DesktopSession&) = delete;
    DesktopSession& operator=(const DesktopSession&) = delete;

    bool load_test(std::string* error = nullptr);
    bool load_file(Ear6SystemType system, const std::string& path, std::string* error = nullptr);
    bool load_state(const std::vector<uint8_t>& state, std::string* error = nullptr);
    bool reset(std::string* error = nullptr);
    bool step(std::string* error = nullptr);

    bool save_state(std::vector<uint8_t>* state, std::string* error = nullptr) const;
    int set_nes_button(Ear6NesButton button, bool pressed);
    void clear_input();
    void clear_audio();

    bool has_content() const;
    bool can_reset() const;
    Ear6SystemType get_system_type() const;
    const std::string& get_display_name() const;
    const std::vector<uint8_t>& get_frame() const;
    int get_frame_width() const;
    int get_frame_height() const;
    AudioRingBuffer& get_audio_buffer();

private:
    enum class SourceType {
        NONE,
        TEST,
        FILE,
        STATE,
    };

    static void audio_callback(const int16_t* data, int num_samples, void* user_data);
    bool replace_context(
        Ear6* candidate,
        Ear6SystemType system,
        SourceType source,
        std::string source_path,
        std::string display_name,
        std::string* error
    );
    void copy_frame();

    Ear6* context_ = nullptr;
    Ear6SystemType system_ = EAR6_SYSTEM_TEST;
    SourceType source_ = SourceType::NONE;
    std::string source_path_;
    std::string display_name_;
    std::vector<uint8_t> frame_;
    int frame_width_ = 0;
    int frame_height_ = 0;
    AudioRingBuffer audio_buffer_;
};

} // namespace ear6::desktop
