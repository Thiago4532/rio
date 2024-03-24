#ifndef _RIO_COMMON_FILE_OPS_HPP
#define _RIO_COMMON_FILE_OPS_HPP

#include "rio/internal/bitwise_base.hpp"
#include <cstdint>

namespace rio {

class file_ops : public internal::bitwise_base<file_ops, std::uint8_t> {
    friend bitwise_base;

    constexpr file_ops(std::uint8_t n) noexcept
        : bitwise_base(n) { }
public:
    constexpr file_ops() noexcept
        : bitwise_base(0x00) { }

    static const file_ops none;
    static const file_ops readable;
    static const file_ops writable;
};

constexpr file_ops file_ops::none     { 0x00 };
constexpr file_ops file_ops::readable { 0x01 };
constexpr file_ops file_ops::writable { 0x02 };

}

#endif // _RIO_COMMON_FILE_OPS_HPP
