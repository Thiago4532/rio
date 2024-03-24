#ifndef _RIO_COMMON_BAD_FILE_DESCRIPTOR_HPP
#define _RIO_COMMON_BAD_FILE_DESCRIPTOR_HPP

#include <stdexcept>

namespace rio {

class bad_file_descriptor : public std::logic_error {
public:
    bad_file_descriptor(const char* what) : std::logic_error(what) { }
    bad_file_descriptor(const std::string& what) : std::logic_error(what) { }
    ~bad_file_descriptor() override = default;
};

}

#endif // _RIO_COMMON_BAD_FILE_DESCRIPTOR_HPP
