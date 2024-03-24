#ifndef _RIO_COMMON_CORO_TRAITS_HPP
#define _RIO_COMMON_CORO_TRAITS_HPP

#include <concepts>
#include <coroutine>

namespace rio {

namespace detail {

template<typename T>
concept is_coroutine_handle = requires(T t) {
    []<typename U>(std::coroutine_handle<U>) { }(t); };

template<typename T>
concept await_suspend_return = std::same_as<T, void>
                            || std::same_as<T, bool>
                            || is_coroutine_handle<T>;
}

template<typename T>
concept Awaiter = requires(T t, std::coroutine_handle<> handle) {
    { t.await_ready() } -> std::same_as<bool>;
    { t.await_suspend(handle) } -> detail::await_suspend_return;
    { t.await_resume() };
};

template<typename T, typename R>
concept AwaiterOf = Awaiter<T> && requires(T t) {
    { t.await_resume() } -> std::convertible_to<R>;
};

template<typename T>
concept Awaitable =
requires(T t) {
    { t.operator co_await() } -> Awaiter;
} || requires(T t) {
    { operator co_await(static_cast<T&&>(t)) } -> Awaiter;
} || Awaiter<T>;

template<typename T, typename R>
concept __AwaitableOf =
requires(T t) {
    { t.operator co_await() } -> AwaiterOf<R>;
} || requires(T t) {
    { operator co_await(static_cast<T&&>(t)) } -> AwaiterOf<R>;
} || AwaiterOf<T, R>;

template<typename T, typename R>
concept AwaitableOf = Awaitable<T>
                   && __AwaitableOf<T, R>;

template<typename T>
concept AwaitCallable = requires(T t) {
    { t() } -> Awaitable;
};

template<typename T, typename R>
concept AwaitCallableOf = AwaitCallable<T> && requires(T t) {
    { t() } -> AwaitableOf<R>;
};

}

#endif // _RIO_COMMON_CORO_TRAITS_HPP
