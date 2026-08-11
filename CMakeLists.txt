cmake_minimum_required(VERSION 3.15)
project(pck_extractor CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/vgmstream
)

target_link_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/vgmstream
)

target_link_libraries(main PRIVATE vgmstream)