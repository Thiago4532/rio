#ifndef _RIO_EVENT_LOOP_HPP
#define _RIO_EVENT_LOOP_HPP

#include <coroutine>
#include <queue>
#include "rio/common/file_ops.hpp"
#include "rio/common/time_type.hpp"
#include "rio/common/coro_traits.hpp"

#include "rio/common/event_loop_exceptions.hpp" // IWYU pragma: export
#include "rio/selector.hpp"

namespace rio {

template<typename T>
concept AwaitSchedulable = Awaitable<T>
                        || AwaitCallable<T>;

template<typename T>
concept Schedulable = std::convertible_to<T, void(*)()>
                   || AwaitSchedulable<T>;

// TODO: Check multiple event loops only when running instead of when constructing
class event_loop_t {
    struct file_internal;

    enum class schedule_type {
        FUNCTION,
        COROUTINE
    };
    class scheduled_handle;
    class schedulable_task;

    class base_awaiter {
    public:
        base_awaiter(event_loop_t& loop, int fd) noexcept
            : loop_(loop), fd_(fd) { }

        bool await_ready() const noexcept {
            return false;
        }

        void await_resume() noexcept { }
    protected:
        event_loop_t& loop_;
        int fd_;
    };
public:
    using schedulable_func_t = void(*)();

    // max_fileno is set to the process's hard limit for the file number
    event_loop_t();

    // max_fileno: hard limit for the file descriptor number
    event_loop_t(std::size_t max_fileno);

    ~event_loop_t();

    event_loop_t(event_loop_t const&) = delete;
    event_loop_t& operator=(event_loop_t const&) = delete;

    static event_loop_t& get() {
        if (!loop_) [[unlikely]]
            throw_bad_event_loop_access();

        return *loop_;
    }

    static event_loop_t* get_or_null() noexcept {
        return loop_;
    }

    static bool exists() noexcept {
        return loop_ != nullptr;
    }
    
    void run();
    void schedule(Schedulable auto&& s, time_type delay = {});
    void schedule_i(schedulable_func_t s, time_type delay = {});
    void schedule_a(AwaitSchedulable auto&& s, time_type delay = {});

    void add_fd(int fd, file_ops ops);
    void del_fd(int fd);

    class read_awaiter final : public base_awaiter {
    public:
        using base_awaiter::base_awaiter;
        void await_suspend(std::coroutine_handle<> coro);
    };
    read_awaiter await_read(int fd) {
        return read_awaiter { *this, fd };
    }

    class write_awaiter final : public base_awaiter {
    public:
        using base_awaiter::base_awaiter;
        void await_suspend(std::coroutine_handle<> coro);
    };
    write_awaiter await_write(int fd) {
        return write_awaiter { *this, fd };
    }

    auto sleep_for(time_type delay) {
        class awaitable {
        public:
            explicit awaitable(event_loop_t& loop, time_type delay) : loop_(loop), delay_(delay) { }

            bool await_ready() const noexcept {
                return false;
            }

            void await_suspend(std::coroutine_handle<> coro) const {
                auto time = time_type::monotonic_clock() + delay_;
                loop_.scheduled_.emplace(coro, time);
            }

            void await_resume() const noexcept { }
        private:
            event_loop_t& loop_;
            time_type delay_;
        };

        return awaitable { *this, delay };
    }

private:
    static event_loop_t *loop_;

    [[noreturn]] static void throw_bad_event_loop_access();
    void ensure_fd_in_range(int fd) const;
    void ensure_fd_registered(int fd) const;

    // TODO: These functions should allow normal functions too, so maybe
    // we should receive a scheduled_handle instead of a coroutine handle.
    void push_read_clb(int fd, std::coroutine_handle<> coro);
    void push_write_clb(int fd, std::coroutine_handle<> coro);

    schedulable_task make_schedulable_task(AwaitSchedulable auto s);

    file_internal* files_;
    std::size_t* constructed_files_;
    selector selector_;

    // ngl, im really considering using another mmap allocation for this,
    // just because it's fun
    std::priority_queue<scheduled_handle,
                        std::vector<scheduled_handle>,
                        std::greater<scheduled_handle>> scheduled_;

    const std::size_t max_fileno_;
};

struct event_loop_t::file_internal {
    file_internal(int fd) noexcept
        : fd_(fd), constructed_(true), valid_(false) { }

    // If file_internal was zero-initialized, it's invalid, so it
    // must be constructed before being used.
    bool is_constructed() const noexcept {
        return constructed_;
    }

    bool is_valid() const noexcept {
        return valid_;
    }

    const int fd_;
    file_ops ops_;
    bool constructed_;
    bool valid_;

    // TODO: Queue is not the right data structure here, we need to be able to
    //      remove elements from the middle of the queue to allow cancelling
    //      coroutines that are waiting for I/O, like when timeouts are reached.
    std::queue<std::coroutine_handle<>> reading_;
    std::queue<std::coroutine_handle<>> writing_;
};

class event_loop_t::scheduled_handle {
public:
    scheduled_handle(std::coroutine_handle<> coro, time_type delay) noexcept
        : type_(schedule_type::COROUTINE), coro_(coro), time_(delay) { }
    scheduled_handle(schedulable_func_t func, time_type time) noexcept
        : type_(schedule_type::FUNCTION), func_(func), time_(time) { }

    void run() {
        switch (type_) {
        case schedule_type::COROUTINE:
            coro_.resume();
            break;
        case schedule_type::FUNCTION:
            func_();
            break;
        }
    }

    schedule_type type() const noexcept {
        return type_;
    }

    time_type time() const noexcept {
        return time_;
    }

    // TODO: priorities
    std::weak_ordering operator<=>(scheduled_handle const& other) const noexcept {
        return time_ <=> other.time_;
    }

private:
    schedule_type type_;
    union {
        std::coroutine_handle<> coro_;
        schedulable_func_t func_;
    };
    time_type time_;
};

class event_loop_t::schedulable_task {
public:
    class promise_type {
    public:
        auto get_return_object() {
            return schedulable_task { std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        auto initial_suspend() noexcept {
            return std::suspend_always {};
        }

        auto final_suspend() noexcept {
            return std::suspend_never {};
        }

        void return_void() noexcept { }

        // TODO: FIX EXCEPTION
        void unhandled_exception() noexcept {
            std::construct_at(std::addressof(exception_), std::current_exception());
        }
    
    private:
        std::exception_ptr exception_;
    };

    void schedule(event_loop_t& loop, time_type delay) {
        auto time = time_type::monotonic_clock() + delay;
        loop.scheduled_.emplace(coro_, time);
    }

    explicit schedulable_task(std::coroutine_handle<promise_type> coro) noexcept
        : coro_(coro) { }

private:
    std::coroutine_handle<promise_type> coro_;
};


template <AwaitSchedulable Schedulable>
event_loop_t::schedulable_task event_loop_t::make_schedulable_task(Schedulable s) {
    if constexpr (Awaitable<Schedulable>) {
        co_await s;
    } else if constexpr (AwaitCallable<Schedulable>) {
        co_await s();
    }
}

template <Schedulable Schedulable>
void event_loop_t::schedule(Schedulable&& s, time_type delay) {
    if constexpr (AwaitSchedulable<Schedulable>) {
        static_assert(!std::convertible_to<Schedulable, void(*)()>,
                "Schedulable type cannot be both invocable and AwaitSchedulable, "
                "use schedule_a or schedule_i instead.");
        static_assert(!(Awaitable<Schedulable> && AwaitCallable<Schedulable>),
                "Schedulable type cannot be both Awaitable and AwaitCallable, use "
                "schedule_a instead.");

        return schedule_a(std::forward<Schedulable>(s), delay);
    } else {
        return schedule_i(std::forward<Schedulable>(s), delay);
    }
}

inline void event_loop_t::schedule_i(schedulable_func_t s, time_type delay) {
    auto time = time_type::monotonic_clock() + delay;
    scheduled_.emplace(s, time);
}

void event_loop_t::schedule_a(AwaitSchedulable auto&& s, time_type delay) {
    auto task = make_schedulable_task(std::forward<decltype(s)>(s));
    task.schedule(*this, delay);
}

// global functions

inline event_loop_t& get_event_loop() {
    return event_loop_t::get();
}

// TODO: Use arguments to improve LSP signature help, but for now
// we can just forward a function to another using template variadic args
#define _FORWARD_TO_LOOP(func) \
    template <typename... Args> \
    inline decltype(auto) func(Args&&... args) requires \
    requires { get_event_loop().func(std::forward<Args>(args)...); } { \
        return get_event_loop().func(std::forward<Args>(args)...); \
    }

_FORWARD_TO_LOOP(schedule);
_FORWARD_TO_LOOP(schedule_i);
_FORWARD_TO_LOOP(schedule_a);
_FORWARD_TO_LOOP(sleep_for);

#undef _FORWARD_TO_LOOP

}


#endif // _RIO_EVENT_LOOP_HPP
