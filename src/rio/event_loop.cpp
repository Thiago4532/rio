#include "rio/event_loop.hpp"
#include "rio/selector.hpp"
#include "tsl/macros.hpp"
#include <cerrno>
#include <ctime>
#include <sys/resource.h>
#include <system_error>
#include <cstring>
#include <sys/mman.h>
#include <format>
#include <memory>
#include "rio/common/bad_file_descriptor.hpp"

#define INLINE extern inline

using std::size_t;

[[noreturn]] static void throw_errno(const char* what) {
    throw std::system_error(errno, std::system_category(), what);
}
#define THROW_ERRNO(msg) [[unlikely]] ::throw_errno(msg)

namespace rio {

event_loop_t* event_loop_t::loop_ = nullptr;

void event_loop_t::throw_bad_event_loop_access() {
    throw bad_event_loop_access();
}

static size_t get_proc_max_fileno() {
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
        THROW_ERRNO("getrlimit(RLIMIT_NOFILE)");
    return rlim.rlim_max;
}

event_loop_t::event_loop_t(): event_loop_t(get_proc_max_fileno()) {}

INLINE void event_loop_t::ensure_fd_in_range(int fd) const {
    if (fd < 0 || static_cast<size_t>(fd) >= max_fileno_)
        throw std::out_of_range(std::format("fd {} is out of range", fd));
}

INLINE void event_loop_t::ensure_fd_registered(int fd) const {
    ensure_fd_in_range(fd);
    if (!files_[fd].is_valid())
        throw bad_file_descriptor(std::format("fd {} is not registered", fd));
}

template <typename T>
static T* page_alloc(size_t num) {
    // assuming page-alignment is always enough
    void* ptr = mmap(nullptr, num * sizeof(T), PROT_READ | PROT_WRITE, // NOLINT
         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 

    // TODO: Use a custom exception based on bad_alloc
    if (ptr == MAP_FAILED)
        THROW_ERRNO("page_alloc");
    return static_cast<T*>(ptr);
}

template <typename T>
static void page_free(T* ptr, size_t num) {
    if (munmap(ptr, num * sizeof(T)) == -1) // NOLINT
        THROW_ERRNO("page_free");
}

event_loop_t::event_loop_t(size_t max_fileno): max_fileno_(max_fileno) {
    if (max_fileno_ == 0)
        throw std::invalid_argument("max_fileno must be > 0");
    if (loop_ != nullptr)
        throw multiple_event_loops_exception();
    loop_ = this;

    files_ = page_alloc<file_internal>(max_fileno_);
    constructed_files_ = page_alloc<size_t>(max_fileno_); // probably not necessary
}

event_loop_t::~event_loop_t() {
    TSL_ASSERT(loop_ == this);

    for (size_t i = 0; constructed_files_[i]; ++i) {
        size_t j = constructed_files_[i] - 1;
        files_[j].~file_internal();
    }
    page_free(constructed_files_, max_fileno_);
    page_free(files_, max_fileno_);
    loop_ = nullptr;
}

void event_loop_t::run() {
    auto pending_events = [this]() -> bool {
        return !scheduled_.empty() || selector_.get_num_events() > 0;
    };

    // Since we have to copy the handles from the queue, I am going to reuse the vector
    // to avoid unnecessary allocations.
    std::vector<std::coroutine_handle<>> awaiting;

    std::vector<selector::event_data> events;
    events.reserve(512);
    while (pending_events()) {
        events.clear();

        if (scheduled_.empty()) {
            selector_.wait(events);
        } else {
            auto timeout = scheduled_.top().time() - time_type::monotonic_clock();
            if (timeout.as_ns() < 0)
                timeout = {};

            selector_.wait(events, timeout);
        }

        auto current_time = time_type::monotonic_clock();
        while (!scheduled_.empty() && scheduled_.top().time() <= current_time) {
            auto sc = scheduled_.top();
            scheduled_.pop();

            sc.run();
        }

        // TODO: A pending event from an old file descriptor may leak to the file descriptor
        // if the file descriptor is removed and re-added while the events are being processed.
        for (auto& ev : events) {
            auto& file = files_[ev.fd];
            // TODO: There is no need to check if the file is valid, since events may be pending the old
            // file descriptor. this is temporary until I implement a way to notify events that the
            // file descriptor was removed.

            if (ev.flags & selector::events::input) {
                awaiting.clear();
                while (!file.reading_.empty()) {
                    awaiting.push_back(file.reading_.front());
                    file.reading_.pop();
                }

                for (auto coro : awaiting)
                    coro.resume();
            }

            if (ev.flags & selector::events::output) {
                awaiting.clear();
                while (!file.writing_.empty()) {
                    awaiting.push_back(file.writing_.front());
                    file.writing_.pop();
                }

                for (auto coro : awaiting)
                    coro.resume();
            }
        }
    }
}

void event_loop_t::add_fd(int fd, file_ops ops) {
    ensure_fd_in_range(fd);

    if (files_[fd].is_valid())
        throw std::invalid_argument(std::format("fd {} is already registered", fd));

    if (!files_[fd].is_constructed())
        std::construct_at(&files_[fd], fd);

    selector::events events = selector::events::none;
    if (ops & file_ops::readable)
        events |= selector::events::input;
    if (ops & file_ops::writable)
        events |= selector::events::output;

    selector_.add_fd(fd, events);
    files_[fd].ops_ = ops;
    files_[fd].valid_ = true;
    files_[fd].reading_ = {};
    files_[fd].writing_ = {};
}

// TODO: If a file descriptor has events pending but is removed from the event loop,
// it may cause the coroutine to hang indefinitely. We should probably wake up coroutines waiting
// for I/O when the file descriptor is removed.
void event_loop_t::del_fd(int fd) {
    ensure_fd_registered(fd);

    selector_.del_fd(fd);
    files_[fd].valid_ = false;
}

void event_loop_t::push_read_clb(int fd, std::coroutine_handle<> coro) {
    ensure_fd_registered(fd);
    if (!(files_[fd].ops_ & file_ops::readable))
        throw bad_file_descriptor(std::format("fd {} is not readable", fd));
    files_[fd].reading_.push(coro);
}

void event_loop_t::push_write_clb(int fd, std::coroutine_handle<> coro) {
    ensure_fd_registered(fd);
    if (!(files_[fd].ops_ & file_ops::writable))
        throw bad_file_descriptor(std::format("fd {} is not writable", fd));
    files_[fd].writing_.push(coro);
}

void event_loop_t::read_awaiter::await_suspend(std::coroutine_handle<> coro) {
    loop_.push_read_clb(fd_, coro);
}

void event_loop_t::write_awaiter::await_suspend(std::coroutine_handle<> coro) {
    loop_.push_write_clb(fd_, coro);
}

}
