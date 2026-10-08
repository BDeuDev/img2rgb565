cmake_minimum_required(VERSION 3.20)

project(img2rgb565 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(img2rgb565
    src/main.c
    src/stb_image.c
    src/stb_image_resize2.c
    src/rle.c
)

target_include_directories(img2rgb565 PRIVATE include)
target_link_libraries(img2rgb565 PRIVATE m)

install(TARGETS img2rgb565
    RUNTIME DESTINATION bin
)