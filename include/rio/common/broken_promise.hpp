#ifndef _RIO_COMMON_BROKEN_PROMISE_HPP
#define _RIO_COMMON_BROKEN_PROMISE_HPP

#include <stdexcept>

namespace rio {

/// \brief
/// Exception thrown when you attempt to retrieve the result of
/// a task that has been detached from its promise/coroutine.
class broken_promise : public std::logic_error {
public:
    broken_promise()
        : std::logic_error("broken promise")
    {}
};

}

#endif // _RIO_COMMON_BROKEN_PROMISE_HPP
