#ifndef _RIO_COMMON_EVENT_LOOP_EXCEPTIONS_HPP
#define _RIO_COMMON_EVENT_LOOP_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

#define _NEW_LOGIC_ERROR(name, msg) \
    class name : public std::logic_error { \
    public: \
        name() : std::logic_error(msg) { } \
        name(const char* what) : std::logic_error(what) { } \
        name(const std::string& what) : std::logic_error(what) { } \
        ~name() override = default; \
    }

namespace rio {

_NEW_LOGIC_ERROR(multiple_event_loops_exception, "multiple event loops exists at the same time");
_NEW_LOGIC_ERROR(bad_event_loop_access, "bad event loop access");

}

#undef _NEW_LOGIC_ERROR

#endif // _RIO_COMMON_EVENT_LOOP_EXCEPTIONS_HPP
