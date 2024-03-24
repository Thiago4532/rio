#include "rio/common/time_type.hpp"

#include "tsl/macros.hpp"

// Although <ctime> is probably enough, I'm not sure if it guarantees that
// clock_gettime will be included, so why not including it :)
// I always get confused when including C headers in C++ files.
#include <time.h>

namespace rio {

static std::timespec gettime(clockid_t id) {
    std::timespec ts;
    auto r = clock_gettime(id, &ts);
    TSL_ASSERT(r >= 0);

    return ts;
}

time_type time_type::monotonic_clock() noexcept {
    return from_timespec(gettime(CLOCK_MONOTONIC_RAW));
}

time_type time_type::hard_monotonic_clock() noexcept {
    return from_timespec(gettime(CLOCK_BOOTTIME));
}

}
