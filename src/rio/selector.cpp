#include "rio/selector.hpp"

#include <cerrno>
#include <cstddef>
#include <memory>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include "tsl/macros.hpp"

using std::size_t;

// Maximum events per wait call, hard-limit so it can be stack allocated.
constexpr size_t MAX_EVENTS = 1024;

#define THROW_IF_UNITIALIZED() [[unlikely]] throw_if_unitialized()

[[noreturn]] static void throw_errno(const char* what) {
    throw std::system_error(errno, std::system_category(), what);
}
#define THROW_ERRNO(msg) [[unlikely]] ::throw_errno(msg)

namespace rio {

const char* bad_selector_access::what() const noexcept {
    return "bad selector access";
}

void selector::throw_bad_selector_access() {
    throw bad_selector_access();
}

selector::selector() : num_events_(0) {
    epfd_ = epoll_create1(0);
    if (epfd_ == -1)
        throw_errno("selector: selector(): epoll_create1");
}

selector::selector(selector&& other) noexcept {
    epfd_ = other.epfd_;
    num_events_ = other.num_events_;
    other.epfd_ = -1;
    other.num_events_ = 0;
}

selector& selector::operator=(selector&& other) noexcept {
    if (this == std::addressof(other))
        return *this;

    destroy();
    epfd_ = other.epfd_;
    num_events_ = other.num_events_;
    other.epfd_ = -1;
    other.num_events_ = 0;
    return *this;
}

void selector::add_fd(int fd, events ev) {
    THROW_IF_UNITIALIZED();

    struct epoll_event epev;
    epev.events = EPOLLET;
    if (ev & events::input)
        epev.events |= (EPOLLIN | EPOLLPRI | EPOLLRDHUP);
    if (ev & events::output)
        epev.events |= EPOLLOUT;
    epev.data.fd = fd;

    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epev) == -1)
        THROW_ERRNO("selector: add_fd: epoll_ctl");
    num_events_++;
} 

void selector::del_fd(int fd) {
    THROW_IF_UNITIALIZED();

    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
        THROW_ERRNO("selector: del_fd: epoll_ctl");
    TSL_ASSERT(num_events_ > 0);
    num_events_--;
}

int selector::wait(std::vector<event_data>& data) {
    return _wait(data, nullptr);
}

int selector::wait(std::vector<event_data>& data, time_type timeout) {
    if (timeout.as_ns() < 0)
        return _wait(data, nullptr);

    std::timespec ts = timeout.as_timespec();
    return _wait(data, &ts);
}

int selector::_wait(std::vector<event_data>& data, std::timespec* timeout) {
    THROW_IF_UNITIALIZED();

    // maybe check if num_events_ is 0? small optimization, but probably useless.
    
    epoll_event events[MAX_EVENTS];
    int n = epoll_pwait2(epfd_, events, MAX_EVENTS, timeout, nullptr);
    if (n == -1) {
        if (errno == EINTR) 
            return 0;
        THROW_ERRNO("selector: wait: epoll_pwait2");
    }

    // I hope EOF sends EPOLLIN or EPOLLPRI :)
    for (int i = 0; i < n; i++) {
        uint32_t mask = events[i].events;

        event_data ev {
            .fd = events[i].data.fd,
            .flags = events::none
        };

        if (mask & EPOLLERR) {
            ev.flags = selector::events::input | selector::events::output;
        } else {
            if (mask & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
                ev.flags |= selector::events::input;
            if (mask & (EPOLLOUT | EPOLLHUP)) // is this correct? :(
                ev.flags |= selector::events::output;
        }

        data.push_back(ev);
    }
    return n;
}

void selector::destroy() noexcept {
    if (epfd_ != -1) 
        ::close(epfd_);
}

selector::~selector() {
    destroy();
}

}
