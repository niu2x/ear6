#include "save_store.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace ear6::desktop {
namespace {

void set_error(std::string* error, const std::string& message) {
    if (error) *error = message;
}

bool inspect_state(
    const std::vector<uint8_t>& state,
    Ear6StateInfo* info,
    std::string* error
) {
    if (state.empty() || !info
        || ear6_get_state_info(state.data(), state.size(), info) != 0) {
        set_error(error, "Invalid or unsupported Ear6 state");
        return false;
    }
    return true;
}

bool read_file(
    const std::filesystem::path& path,
    std::vector<uint8_t>* data,
    std::string* error
) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        set_error(error, "Unable to open " + path.string());
        return false;
    }
    const std::streamsize size = input.tellg();
    if (size < 0) {
        set_error(error, "Unable to read " + path.string());
        return false;
    }
    input.seekg(0);
    data->resize(static_cast<size_t>(size));
    if (size > 0 && !input.read(reinterpret_cast<char*>(data->data()), size)) {
        data->clear();
        set_error(error, "Unable to read " + path.string());
        return false;
    }
    return true;
}

std::chrono::system_clock::time_point to_system_time(
    std::filesystem::file_time_type value
) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now()
    );
}

} // namespace

SaveStore::SaveStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

const std::filesystem::path& SaveStore::get_directory() const {
    return directory_;
}

std::filesystem::path SaveStore::state_path(
    Ear6SystemType system_type,
    uint64_t content_identity
) const {
    std::ostringstream filename;
    filename << static_cast<int>(system_type) << '-' << std::hex << std::setw(16)
             << std::setfill('0') << content_identity << ".e6s";
    return directory_ / filename.str();
}

bool SaveStore::save(
    const std::vector<uint8_t>& state,
    std::filesystem::path* path,
    std::string* error
) const {
    if (error) error->clear();
    Ear6StateInfo info = {};
    if (!inspect_state(state, &info, error)) return false;

    std::error_code filesystem_error;
    std::filesystem::create_directories(directory_, filesystem_error);
    if (filesystem_error) {
        set_error(error, "Unable to create save directory: " + filesystem_error.message());
        return false;
    }

    const std::filesystem::path destination = state_path(info.system_type, info.content_identity);
    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output
            || !output.write(
                reinterpret_cast<const char*>(state.data()),
                static_cast<std::streamsize>(state.size())
            )
            || !output.flush()) {
            output.close();
            std::filesystem::remove(temporary, filesystem_error);
            set_error(error, "Unable to write save state");
            return false;
        }
    }

    if (std::rename(temporary.c_str(), destination.c_str()) != 0) {
        std::filesystem::remove(temporary, filesystem_error);
        set_error(error, "Unable to replace save state");
        return false;
    }
    if (path) *path = destination;
    return true;
}

std::vector<SaveEntry> SaveStore::list(std::string* error) const {
    if (error) error->clear();
    std::vector<SaveEntry> entries;
    std::error_code filesystem_error;
    if (!std::filesystem::exists(directory_, filesystem_error)) return entries;

    for (const auto& file : std::filesystem::directory_iterator(directory_, filesystem_error)) {
        if (filesystem_error) break;
        if (!file.is_regular_file() || file.path().extension() != ".e6s") continue;

        std::vector<uint8_t> state;
        Ear6StateInfo info = {};
        if (!read_file(file.path(), &state, nullptr) || !inspect_state(state, &info, nullptr)) {
            continue;
        }

        SaveEntry entry;
        entry.system_type = info.system_type;
        entry.content_identity = info.content_identity;
        entry.display_name = info.content_name_hint && info.content_name_hint_size > 0
            ? std::string(info.content_name_hint, info.content_name_hint_size)
            : "Saved State";
        entry.path = file.path();
        entry.saved_at = to_system_time(file.last_write_time());
        if (info.preview_format == EAR6_STATE_PREVIEW_RGBA8888 && info.preview_data
            && info.preview_size > 0 && info.preview_width > 0 && info.preview_height > 0) {
            entry.preview.assign(info.preview_data, info.preview_data + info.preview_size);
            entry.preview_width = info.preview_width;
            entry.preview_height = info.preview_height;
        }
        entries.push_back(std::move(entry));
    }

    if (filesystem_error) {
        set_error(error, "Unable to scan save directory: " + filesystem_error.message());
    }
    std::sort(entries.begin(), entries.end(), [](const SaveEntry& left, const SaveEntry& right) {
        return left.saved_at > right.saved_at;
    });
    return entries;
}

bool SaveStore::load(
    const SaveEntry& entry,
    std::vector<uint8_t>* state,
    std::string* error
) const {
    if (error) error->clear();
    if (!state) {
        set_error(error, "Missing output buffer");
        return false;
    }

    std::vector<uint8_t> candidate;
    Ear6StateInfo info = {};
    if (!read_file(entry.path, &candidate, error) || !inspect_state(candidate, &info, error)) {
        return false;
    }
    if (info.system_type != entry.system_type
        || info.content_identity != entry.content_identity) {
        set_error(error, "Save identity mismatch");
        return false;
    }
    *state = std::move(candidate);
    return true;
}

} // namespace ear6::desktop
