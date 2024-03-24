#ifndef _RIO_COMMON_TIME_TYPE_HPP
#define _RIO_COMMON_TIME_TYPE_HPP

#include <cstdint>
#include <ctime>
#include <chrono>

namespace rio {

// TODO: Refactor/Reimplement
// We should create a duration_type too, to differentiate between
// time and duration.

class time_type {
    constexpr explicit time_type(std::int64_t t) noexcept:
        time_ {t} { }

    constexpr static std::int64_t fdiv(std::int64_t a, std::int64_t b) noexcept {
        if ((a < 0) != (b < 0))
            a -= (b - 1);
        return a / b;
    }
public:
    constexpr time_type() noexcept:
        time_ {0} { }

    template<typename Rep, typename Period>
    constexpr time_type(const std::chrono::duration<Rep, Period>& d) noexcept:
        time_ {std::chrono::duration_cast<std::chrono::nanoseconds>(d).count()} { }
    
    // no precision loss
    constexpr static time_type from_ns(std::int64_t i) noexcept {
        return time_type { i };
    }

    constexpr static time_type from_sec(std::time_t seconds) noexcept {
        return time_type { seconds * 1'000'000'000ll };
    }

    constexpr static time_type from_ms(std::int64_t ms) noexcept {
        return time_type { ms * 1'000'000ll };
    }

    constexpr static time_type from_timespec(std::timespec const& ts) noexcept {
        return time_type { ts.tv_sec * 1'000'000'000ll + ts.tv_nsec };
    }

    // no precision loss
    constexpr std::int64_t as_ns() const noexcept {
        return time_;
    }

    // rounding down to seconds
    constexpr std::time_t as_sec() const noexcept {
        return fdiv(time_, 1'000'000'000ll);
    }
    
    // truncate to milliseconds
    constexpr std::int64_t as_ms() const noexcept {
        return fdiv(time_, 1'000'000ll);
    }

    constexpr std::timespec as_timespec() const noexcept {
        auto sec = fdiv(time_, 1'000'000'000ll);
        auto nsec = time_ - 1'000'000'000ll * sec;

        return std::timespec { 
            .tv_sec = sec,
            .tv_nsec = nsec
        };
    }

    constexpr double as_dsec() const noexcept {
        return time_ / 1'000'000'000.0;
    }
    
    constexpr time_type operator+(time_type other) const noexcept {
        return time_type { time_ + other.time_ };
    }

    constexpr time_type& operator+=(time_type other) noexcept {
        time_ += other.time_;
        return *this;
    }

    constexpr time_type operator-(time_type other) const noexcept {
        return time_type { time_ - other.time_ };
    }

    constexpr time_type& operator-=(time_type other) noexcept {
        time_ -= other.time_;
        return *this;
    }

    constexpr auto operator<=>(time_type other) const noexcept {
        return time_ <=> other.time_;
    }

    constexpr time_type operator+() const noexcept {
        return time_type { time_ };
    }

    constexpr time_type operator-() const noexcept {
        return time_type { -time_ };
    }

    // A monotonic clock that doesn't run while the system is suspended.
    static time_type monotonic_clock() noexcept;

    // Like `monotonic_clock`, but it runs while the system is suspended.
    static time_type hard_monotonic_clock() noexcept;

private:
    std::int64_t time_;
};

}

#endif // _RIO_COMMON_TIME_TYPE_HPP
