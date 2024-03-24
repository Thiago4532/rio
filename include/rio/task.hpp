#ifndef _RIO_TASK_HPP
#define _RIO_TASK_HPP

#include <coroutine>
#include <cstdint>
#include <memory>
#include <exception>
#include "tsl/macros.hpp"
#include "rio/common/broken_promise.hpp"

namespace rio::impl_task {

using std::coroutine_handle;
using std::uint8_t;

// TODO: clang::coro_return_type attribute
template<typename T>
class task;

class task_promise_base {
    struct final_awaitable {
        bool await_ready() const noexcept {
            return false;
        }

        template<typename PROMISE> 
        coroutine_handle<> await_suspend(coroutine_handle<PROMISE> coro) const noexcept {
            return coro.promise().continuation_;
        }

        void await_resume() const noexcept {}
    };

public:
    task_promise_base() noexcept {}

    auto initial_suspend() const noexcept {
        return std::suspend_always {};
    }

    auto final_suspend() const noexcept {
        return final_awaitable {};
    }

private:
    template<typename T>
    friend class task;

    coroutine_handle<> continuation_;
};

template<typename T>
class task_promise final : public task_promise_base {
public:
    task_promise() noexcept {}

    ~task_promise() {
        switch (resultType_) {
        case result_type::value:
            value_.~T();
            break;
        case result_type::exception:
            exception_.~exception_ptr();
            break;
        default:
            break;
        }
    }

    task<T> get_return_object() noexcept {
        return task<T> (coroutine_handle<task_promise>::from_promise(*this));
    }

    void unhandled_exception() noexcept {
        std::construct_at(std::addressof(exception_), std::current_exception());
        resultType_ = result_type::exception;
    }

    // TODO: How to keep warnings when returning? Specially lifetime-based info, like returning
    // a string_view from a temporary std::string
    template<std::convertible_to<T> U>
    void return_value (U&& value) noexcept(std::is_nothrow_constructible_v<T, U>) {
        std::construct_at(std::addressof(value_), std::forward<U>(value));
        resultType_ = result_type::value;
    }

    T& result() & {
        if (resultType_ == result_type::exception)
            std::rethrow_exception(exception_);

        TSL_ASSERT(resultType_ == result_type::value);
        return value_;
    }

    T&& result() && {
        return std::move(this->result());
    }

private:
    enum class result_type : uint8_t { empty, value, exception };
    result_type resultType_ = result_type::empty;

    union {
        T value_;
        std::exception_ptr exception_;
    };
};

template<>
class task_promise<void> final : public task_promise_base {
public:
    task_promise() noexcept = default;

    task<void> get_return_object() noexcept;

    void unhandled_exception() noexcept {
        std::construct_at(std::addressof(exception_), std::current_exception());
    }

    void return_void () noexcept {}

    void result() {
        if (exception_)
            std::rethrow_exception(exception_);
    }

private:
    std::exception_ptr exception_;
};

template<typename T> requires std::is_reference_v<T>
class task_promise<T> final : public task_promise_base {
    using value_type = std::remove_reference_t<T>;
public:
    task_promise() noexcept = default;

    task<T> get_return_object() noexcept {
        return task<T> (coroutine_handle<task_promise>::from_promise(*this));
    }

    void unhandled_exception() noexcept {
        std::construct_at(std::addressof(exception_), std::current_exception());
    }

    void return_value (T value) noexcept {
        value_ = std::addressof(value);
    }

    T result() {
        if (exception_)
            std::rethrow_exception(exception_);

        TSL_ASSERT(value_ != nullptr);
        return value_;
    }

private:
    value_type* value_ = nullptr;
    std::exception_ptr exception_;
};

template<typename T = void>
class [[nodiscard]] task {
public:
    using promise_type = task_promise<T>;
    using value_type = T;

private:
    struct awaitable_base {
        coroutine_handle<promise_type> coroutine_;

        awaitable_base(coroutine_handle<promise_type> coroutine) noexcept
            : coroutine_(coroutine) { }

        bool await_ready() const noexcept {
            return !coroutine_ || coroutine_.done();
        }

        coroutine_handle<> await_suspend(coroutine_handle<> coro) noexcept {
            coroutine_.promise().continuation_ = coro;
            return coroutine_;
        }
    };

public:
    task() noexcept
        : coroutine_(nullptr) {}

    // TODO: Maybe make this constructor private?
    explicit task(coroutine_handle<promise_type> coroutine)
        : coroutine_(coroutine) {}

    ~task() {
        if (coroutine_)
            coroutine_.destroy();
    }

    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task(task&& other) noexcept
        : coroutine_(other.coroutine_)
    {
        other.coroutine_ = nullptr;
    }

    task& operator=(task&& other) noexcept {
        if (std::addressof(other) != this) {
            if (coroutine_)
                coroutine_.destroy();

            coroutine_ = other.coroutine_;
            other.coroutine_ = nullptr;
        }

        return *this;
    }

    bool is_ready() const noexcept {
        return !coroutine_ || coroutine_.done();
    }

    auto operator co_await() & noexcept {
        struct awaitable : awaitable_base {
            using awaitable_base::awaitable_base;

            decltype(auto) await_resume() {
                if (!this->coroutine_)
                    throw broken_promise {};

                return this->coroutine_.promise().result();
            }
        };
        
        return awaitable { coroutine_ };
    }

    auto operator co_await() && noexcept {
        struct awaitable : awaitable_base {
            using awaitable_base::awaitable_base;

            decltype(auto) await_resume() {
                if (!this->coroutine_)
                    throw broken_promise {};

                return std::move(this->coroutine_.promise()).result();
            }
        };
        
        return awaitable { coroutine_ };
    }

    auto when_ready() noexcept {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            void await_resume() const noexcept {}
        };

        return awaitable { coroutine_ };
    }
// private:
public:
    coroutine_handle<promise_type> coroutine_;
};

inline task<void> task_promise<void>::get_return_object() noexcept {
    return task<void> (coroutine_handle<task_promise>::from_promise(*this));
}

}

// Export
namespace rio {

using impl_task::task;

}

#endif // _RIO_TASK_HPP
