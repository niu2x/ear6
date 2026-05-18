#pragma once

#include <cstdint>

namespace ear6::nes {

class DatachBarcodeReader {
public:
    DatachBarcodeReader() = default;

    uint8_t get_output() {
        return 0;
    }
};

} // namespace ear6::nes
