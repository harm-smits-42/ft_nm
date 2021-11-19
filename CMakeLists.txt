cmake_minimum_required(VERSION 3.16)
project(ft_nm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

add_subdirectory(libft)

file(GLOB_RECURSE SOURCES src/**.c)
add_executable(ft_nm ${SOURCES})
target_link_libraries(ft_nm libft)
include_directories(inc)