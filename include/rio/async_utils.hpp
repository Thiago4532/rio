#ifndef _RIO_ASYNC_UTILS_HPP
#define _RIO_ASYNC_UTILS_HPP

#include "rio/event_loop.hpp"

namespace rio {

inline auto sleep_for(time_type delay) {
    class awaitable {
    public:
        explicit awaitable(time_type delay) : delay_(delay) { }

        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> coro) const {
            schedule_handle(coro, delay_);
        }

        void await_resume() const noexcept { }
    private:
        time_type delay_;
    };

    return awaitable { delay };
}

}

#endif // _RIO_ASYNC_UTILS_HPP
