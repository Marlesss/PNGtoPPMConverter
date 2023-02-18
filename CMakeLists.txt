cmake_minimum_required(VERSION 3.19)
project(c_2_Marlesss C)

set(CMAKE_C_STANDARD 11)

include_directories("zlib/")
include_directories("libdeflatep/")
include_directories("isalp/")
link_directories(D:/CppCodes/c_2-Marlesss)

add_definitions(-DISAL)

add_executable(main main.c)
target_link_libraries(main z -llibdeflate)