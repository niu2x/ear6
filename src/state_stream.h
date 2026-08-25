#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace ear6 {

template<typename T, bool = std::is_enum_v<T>>
struct StateValueType {
    using type = T;
};

template<typename T>
struct StateValueType<T, true> {
    using type = std::underlying_type_t<T>;
};

class StateStream {
public:
    StateStream() = default;

    StateStream(const void* data, size_t size)
        : input_(static_cast<const uint8_t*>(data)), input_size_(size), loading_(true) {}

    bool is_loading() const { return loading_; }
    bool is_saving() const { return !loading_; }
    bool has_error() const { return error_; }
    void fail() { error_ = true; }
    size_t get_remaining() const { return input_size_ - position_; }
    const std::vector<uint8_t>& get_data() const { return output_; }

    template<typename T>
    void sync(T& value) {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        using ValueType = typename StateValueType<T>::type;
        using UnsignedType = std::make_unsigned_t<ValueType>;

        if (is_saving()) {
            UnsignedType encoded = static_cast<UnsignedType>(value);
            for (size_t i = 0; i < sizeof(UnsignedType); ++i) {
                output_.push_back(static_cast<uint8_t>(encoded >> (i * 8)));
            }
            return;
        }

        if (get_remaining() < sizeof(UnsignedType)) {
            error_ = true;
            return;
        }

        UnsignedType decoded = 0;
        for (size_t i = 0; i < sizeof(UnsignedType); ++i) {
            decoded |= static_cast<UnsignedType>(input_[position_++]) << (i * 8);
        }
        value = static_cast<T>(decoded);
    }

    void sync(bool& value) {
        uint8_t encoded = value ? 1 : 0;
        sync(encoded);
        if (is_loading()) {
            if (encoded > 1) {
                error_ = true;
            } else {
                value = encoded != 0;
            }
        }
    }

    void sync_bytes(void* data, size_t size) {
        if (is_saving()) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            output_.insert(output_.end(), bytes, bytes + size);
            return;
        }

        if (get_remaining() < size) {
            error_ = true;
            return;
        }
        std::memcpy(data, input_ + position_, size);
        position_ += size;
    }

    template<typename T, size_t Size>
    void sync_array(T (&values)[Size]) {
        for (T& value : values) {
            sync(value);
        }
    }

    template<typename T>
    void sync_span(T* values, size_t size) {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        for (size_t i = 0; i < size; ++i) {
            sync(values[i]);
        }
    }

    template<typename T>
    void sync_vector(std::vector<T>& values) {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        uint64_t size = values.size();
        sync(size);
        if (has_error()) return;

        if (is_loading()) {
            if (size > get_remaining() / sizeof(T)
                || size > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
                error_ = true;
                return;
            }
            values.resize(static_cast<size_t>(size));
        }
        for (T& value : values) {
            sync(value);
        }
    }

private:
    const uint8_t* input_ = nullptr;
    size_t input_size_ = 0;
    size_t position_ = 0;
    bool loading_ = false;
    bool error_ = false;
    std::vector<uint8_t> output_;
};

} // namespace ear6
