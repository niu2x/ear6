#pragma once

#include <ear6/ear6.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ear6::desktop {

struct SaveEntry {
    Ear6SystemType system_type = EAR6_SYSTEM_TEST;
    uint64_t content_identity = 0;
    std::string display_name;
    std::filesystem::path path;
    std::chrono::system_clock::time_point saved_at;
    std::vector<uint8_t> preview;
    int preview_width = 0;
    int preview_height = 0;
};

class SaveStore {
public:
    explicit SaveStore(std::filesystem::path directory);

    const std::filesystem::path& get_directory() const;
    bool save(
        const std::vector<uint8_t>& state,
        std::filesystem::path* path,
        std::string* error
    ) const;
    std::vector<SaveEntry> list(std::string* error = nullptr) const;
    bool load(const SaveEntry& entry, std::vector<uint8_t>* state, std::string* error) const;

private:
    std::filesystem::path state_path(
        Ear6SystemType system_type,
        uint64_t content_identity
    ) const;

    std::filesystem::path directory_;
};

} // namespace ear6::desktop
