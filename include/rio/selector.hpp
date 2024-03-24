#ifndef _RIO_SELECTOR_HPP
#define _RIO_SELECTOR_HPP

#include <cstdint>
#include <exception>
#include <vector>
#include "rio/internal/bitwise_base.hpp"
#include "rio/common/time_type.hpp"

namespace rio {

class bad_selector_access : public std::exception {
public:
    bad_selector_access() = default;
    ~bad_selector_access() override = default;

    const char* what() const noexcept override;
};

class selector {
    struct no_init_t {
        explicit no_init_t(int) noexcept { }
    };
public:
    inline static no_init_t no_init { 0 };

    class events;
    struct event_data;
public:
    selector();
    selector(no_init_t) noexcept: epfd_(-1), num_events_(0) { }
    selector(selector const&) = delete;
    selector& operator=(selector const&) = delete;

    selector(selector&& other) noexcept;
    selector& operator=(selector&&) noexcept;

    void add_fd(int fd, events ev);
    void del_fd(int fd);
    int wait(std::vector<event_data>& data);
    int wait(std::vector<event_data>& data, time_type timeout);

    std::size_t get_num_events() const noexcept {
        return num_events_;
    }

    void destroy() noexcept;
    ~selector();

private:
    [[noreturn]] static void throw_bad_selector_access();
    int _wait(std::vector<event_data>& data, std::timespec* timeout);

    void throw_if_unitialized() {
        if (epfd_ == -1)
            throw_bad_selector_access();
    }

    int epfd_;
    std::size_t num_events_;
};

class selector::events : public internal::bitwise_base<selector::events, std::uint8_t> {
    friend bitwise_base;

    constexpr events(std::uint8_t n) noexcept
        : bitwise_base(n) { }
public:
    constexpr events() noexcept
        : bitwise_base(0x00) { }

    static const events none;
    static const events input;
    static const events output;
};

constexpr selector::events selector::events::none   { 0x00 };
constexpr selector::events selector::events::input  { 0x01 };
constexpr selector::events selector::events::output { 0x02 };

struct selector::event_data {
    int fd;
    events flags;
};

}

#endif // _RIO_SELECTOR_HPP
