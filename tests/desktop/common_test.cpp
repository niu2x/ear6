#include "audio_ring_buffer.h"
#include "desktop_session.h"
#include "save_store.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using ear6::desktop::AudioRingBuffer;
using ear6::desktop::DesktopSession;
using ear6::desktop::SaveStore;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("ear6-desktop-test-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& get_path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

TEST(AudioRingBuffer, WrapsAndPreservesSampleOrder) {
    AudioRingBuffer buffer(4);
    const int16_t first[] = {1, 2, 3};
    buffer.push(first, 3);

    int16_t output[4] = {};
    ASSERT_EQ(buffer.pop(output, 2), 2);
    EXPECT_EQ(output[0], 1);
    EXPECT_EQ(output[1], 2);

    const int16_t second[] = {4, 5, 6};
    buffer.push(second, 3);
    ASSERT_EQ(buffer.pop(output, 4), 4);
    EXPECT_EQ(std::vector<int16_t>(output, output + 4), (std::vector<int16_t>{3, 4, 5, 6}));
}

TEST(AudioRingBuffer, OverflowKeepsNewestSamples) {
    AudioRingBuffer buffer(4);
    const int16_t input[] = {1, 2, 3, 4, 5, 6};
    buffer.push(input, 6);

    int16_t output[4] = {};
    ASSERT_EQ(buffer.pop(output, 4), 4);
    EXPECT_EQ(std::vector<int16_t>(output, output + 4), (std::vector<int16_t>{3, 4, 5, 6}));
}

TEST(DesktopSession, TestSystemStepsAndRestoresAState) {
    DesktopSession session;
    std::string error;
    ASSERT_TRUE(session.load_test(&error)) << error;
    EXPECT_TRUE(session.has_content());
    EXPECT_TRUE(session.can_reset());
    const std::vector<uint8_t> initial_frame = session.get_frame();

    ASSERT_TRUE(session.step(&error)) << error;
    EXPECT_GT(session.get_frame_width(), 0);
    EXPECT_GT(session.get_frame_height(), 0);
    EXPECT_EQ(
        session.get_frame().size(),
        static_cast<size_t>(session.get_frame_width() * session.get_frame_height() * 4)
    );
    EXPECT_NE(session.get_frame(), initial_frame);

    std::vector<uint8_t> state;
    ASSERT_TRUE(session.save_state(&state, &error)) << error;
    ASSERT_FALSE(state.empty());
    ASSERT_TRUE(session.load_state(state, &error)) << error;
    EXPECT_TRUE(session.has_content());
    EXPECT_FALSE(session.can_reset());
    EXPECT_FALSE(session.get_frame().empty());
}

TEST(SaveStore, SameContentUsesOneAtomicSlot) {
    TemporaryDirectory directory;
    SaveStore store(directory.get_path());
    DesktopSession session;
    std::string error;
    ASSERT_TRUE(session.load_test(&error)) << error;

    ASSERT_TRUE(session.step(&error)) << error;
    std::vector<uint8_t> first;
    ASSERT_TRUE(session.save_state(&first, &error)) << error;
    std::filesystem::path first_path;
    ASSERT_TRUE(store.save(first, &first_path, &error)) << error;

    ASSERT_TRUE(session.step(&error)) << error;
    std::vector<uint8_t> second;
    ASSERT_TRUE(session.save_state(&second, &error)) << error;
    std::filesystem::path second_path;
    ASSERT_TRUE(store.save(second, &second_path, &error)) << error;

    EXPECT_EQ(first_path, second_path);
    const auto entries = store.list(&error);
    ASSERT_EQ(entries.size(), 1) << error;
    EXPECT_FALSE(entries.front().preview.empty());

    std::vector<uint8_t> restored;
    ASSERT_TRUE(store.load(entries.front(), &restored, &error)) << error;
    EXPECT_EQ(restored, second);
}

} // namespace
