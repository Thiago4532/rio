#ifndef _RIO_INTERNAL_BITWISE_BASE_HPP
#define _RIO_INTERNAL_BITWISE_BASE_HPP

#include <concepts>

namespace rio::internal {

template<class T, std::unsigned_integral U>
class bitwise_base {
    friend T;
    U n_;

    T& self() noexcept {
        return static_cast<T&>(*this);
    }

    T const& self() const noexcept {
        return static_cast<T const&>(*this);
    }
public:
    using underlying_type = U;

    constexpr bitwise_base() = default;
    constexpr bitwise_base(U n) noexcept : n_(n) { }

    [[nodiscard]] constexpr U as_num() const noexcept {
        return n_;
    }

    [[nodiscard]] explicit operator U() const noexcept {
        return n_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return !!n_;
    }

    [[nodiscard]] T operator!() const noexcept {
        return !n_;
    }

    [[nodiscard]] T operator&(T other) const noexcept {
        return n_ & other.n_;
    }

    T& operator&=(T other) noexcept {
        n_ &= other.n_;
        return self();
    }

    [[nodiscard]] T operator|(T other) const noexcept {
        return n_ | other.n_;
    }

    T& operator|=(T other) noexcept {
        n_ |= other.n_;
        return self();
    }

    [[nodiscard]] T operator^(T other) const noexcept {
        return n_ ^ other.n_;
    }

    T& operator^=(T other) noexcept {
        n_ ^= other.n_;
        return self();
    }

    [[nodiscard]] T operator~() const noexcept {
        return ~n_;
    }

};

}



#endif // _RIO_INTERNAL_BITWISE_BASE_HPP
