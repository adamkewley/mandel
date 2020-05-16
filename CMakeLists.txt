cmake_minimum_required(VERSION 3.13)
project(mandel)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(mandel
        main.cpp
)
target_include_directories(mandel PRIVATE
        ${SDL2_INCLUDE_DIR}
        ${SDL2_TTF_INCLUDE_DIRS}
)
target_link_libraries(mandel
        ${SDL2_LIBRARY}
        ${SDL2_TTF_LIBRARIES}
)
