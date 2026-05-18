#pragma once

#include <cstdint>
#include <cstring>

namespace ear6::nes {

class BaseEeprom24C0X {
protected:
    enum class Mode : uint8_t {
        Idle = 0,
        Address = 1,
        Read = 2,
        Write = 3,
        SendAck = 4,
        WaitAck = 5,
        ChipAddress = 6
    };

    Mode mode_ = Mode::Idle;
    Mode next_mode_ = Mode::Idle;
    uint8_t chip_address_ = 0;
    uint8_t address_ = 0;
    uint8_t data_ = 0;
    uint8_t counter_ = 0;
    uint8_t output_ = 0;
    uint8_t prev_scl_ = 0;
    uint8_t prev_sda_ = 0;
    uint8_t rom_data_[256] = {};

public:
    virtual ~BaseEeprom24C0X() = default;
    virtual void write(uint8_t scl, uint8_t sda) = 0;

    uint8_t read() { return output_; }
    void write_scl(uint8_t scl) { write(scl, prev_sda_); }
    void write_sda(uint8_t sda) { write(prev_scl_, sda); }
};

class Eeprom24C01 : public BaseEeprom24C0X {
private:
    void write_bit(uint8_t& dest, uint8_t value) {
        if (counter_ < 8) {
            uint8_t mask = ~(1 << counter_);
            dest = (dest & mask) | (value << counter_);
            counter_++;
        }
    }

    void read_bit() {
        if (counter_ < 8) {
            output_ = (data_ & (1 << counter_)) ? 1 : 0;
            counter_++;
        }
    }

public:
    Eeprom24C01() {
        std::memset(rom_data_, 0, 128);
    }

    void write(uint8_t scl, uint8_t sda) override {
        if (prev_scl_ && scl && sda < prev_sda_) {
            mode_ = Mode::Address;
            address_ = 0;
            counter_ = 0;
            output_ = 1;
        } else if (prev_scl_ && scl && sda > prev_sda_) {
            mode_ = Mode::Idle;
            output_ = 1;
        } else if (scl > prev_scl_) {
            switch (mode_) {
                case Mode::Address:
                    if (counter_ < 7) {
                        write_bit(address_, sda);
                    } else if (counter_ == 7) {
                        counter_ = 8;
                        if (sda) {
                            next_mode_ = Mode::Read;
                            data_ = rom_data_[address_ & 0x7F];
                        } else {
                            next_mode_ = Mode::Write;
                        }
                    }
                    break;
                case Mode::SendAck:
                    output_ = 0;
                    break;
                case Mode::Read:
                    read_bit();
                    break;
                case Mode::Write:
                    write_bit(data_, sda);
                    break;
                case Mode::WaitAck:
                    if (!sda) {
                        next_mode_ = Mode::Idle;
                    }
                    break;
                default:
                    break;
            }
        } else if (scl < prev_scl_) {
            switch (mode_) {
                case Mode::Address:
                    if (counter_ == 8) {
                        mode_ = Mode::SendAck;
                        output_ = 1;
                    }
                    break;
                case Mode::SendAck:
                    mode_ = next_mode_;
                    counter_ = 0;
                    output_ = 1;
                    break;
                case Mode::Read:
                    if (counter_ == 8) {
                        mode_ = Mode::WaitAck;
                        address_ = (address_ + 1) & 0x7F;
                    }
                    break;
                case Mode::Write:
                    if (counter_ == 8) {
                        mode_ = Mode::SendAck;
                        next_mode_ = Mode::Idle;
                        rom_data_[address_ & 0x7F] = data_;
                        address_ = (address_ + 1) & 0x7F;
                    }
                    break;
                default:
                    break;
            }
        }
        prev_scl_ = scl;
        prev_sda_ = sda;
    }
};

class Eeprom24C02 : public BaseEeprom24C0X {
private:
    void write_bit(uint8_t& dest, uint8_t value) {
        if (counter_ < 8) {
            uint8_t mask = ~(1 << (7 - counter_));
            dest = (dest & mask) | (value << (7 - counter_));
            counter_++;
        }
    }

    void read_bit() {
        if (counter_ < 8) {
            output_ = (data_ & (1 << (7 - counter_))) ? 1 : 0;
            counter_++;
        }
    }

public:
    Eeprom24C02() {
        std::memset(rom_data_, 0, 256);
    }

    void write(uint8_t scl, uint8_t sda) override {
        if (prev_scl_ && scl && sda < prev_sda_) {
            mode_ = Mode::ChipAddress;
            counter_ = 0;
            output_ = 1;
        } else if (prev_scl_ && scl && sda > prev_sda_) {
            mode_ = Mode::Idle;
            output_ = 1;
        } else if (scl > prev_scl_) {
            switch (mode_) {
                default:
                    break;
                case Mode::ChipAddress:
                    write_bit(chip_address_, sda);
                    break;
                case Mode::Address:
                    write_bit(address_, sda);
                    break;
                case Mode::Read:
                    read_bit();
                    break;
                case Mode::Write:
                    write_bit(data_, sda);
                    break;
                case Mode::SendAck:
                    output_ = 0;
                    break;
                case Mode::WaitAck:
                    if (!sda) {
                        next_mode_ = Mode::Read;
                        data_ = rom_data_[address_];
                    }
                    break;
            }
        } else if (scl < prev_scl_) {
            switch (mode_) {
                case Mode::ChipAddress:
                    if (counter_ == 8) {
                        if ((chip_address_ & 0xA0) == 0xA0) {
                            mode_ = Mode::SendAck;
                            counter_ = 0;
                            output_ = 1;
                            if (chip_address_ & 0x01) {
                                next_mode_ = Mode::Read;
                                data_ = rom_data_[address_];
                            } else {
                                next_mode_ = Mode::Address;
                            }
                        } else {
                            mode_ = Mode::Idle;
                            counter_ = 0;
                            output_ = 1;
                        }
                    }
                    break;
                case Mode::Address:
                    if (counter_ == 8) {
                        counter_ = 0;
                        mode_ = Mode::SendAck;
                        next_mode_ = Mode::Write;
                        output_ = 1;
                    }
                    break;
                case Mode::Read:
                    if (counter_ == 8) {
                        mode_ = Mode::WaitAck;
                        address_ = (address_ + 1) & 0xFF;
                    }
                    break;
                case Mode::Write:
                    if (counter_ == 8) {
                        counter_ = 0;
                        mode_ = Mode::SendAck;
                        next_mode_ = Mode::Write;
                        rom_data_[address_] = data_;
                        address_ = (address_ + 1) & 0xFF;
                    }
                    break;
                case Mode::SendAck:
                case Mode::WaitAck:
                    mode_ = next_mode_;
                    counter_ = 0;
                    output_ = 1;
                    break;
                default:
                    break;
            }
        }
        prev_scl_ = scl;
        prev_sda_ = sda;
    }
};

} // namespace ear6::nes
