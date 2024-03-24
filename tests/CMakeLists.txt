add_executable(main main.cpp)
target_link_libraries(main PRIVATE rio)

# Enable warnings.
if (RIO_MASTER_PROJECT)
  if (MSVC)
    target_compile_options(rio INTERFACE /W4)
  else()
    target_compile_options(rio PRIVATE -Wall -Wextra -pedantic)
  endif()
endif()
