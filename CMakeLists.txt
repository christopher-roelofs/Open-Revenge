cmake_minimum_required(VERSION 3.10)
project(rodent C)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

add_executable(rodent
    src/main.c
    src/menu.c
    src/settings.c
    src/platform.c
    src/field.c
    src/game.c
    src/pack.c
    src/assets.c
    src/text.c
)

target_include_directories(rodent PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

target_link_libraries(rodent
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    m
)

target_compile_options(rodent PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
)

# goal_test: SDL_VIDEODRIVER=dummy build/goal_test tests/packs (from the project root)
add_executable(goal_test
    tests/goal_test.c
    src/platform.c
    src/field.c
    src/game.c
    src/pack.c
    src/assets.c
    src/text.c
)
target_include_directories(goal_test PRIVATE src ${CMAKE_SOURCE_DIR}/third_party ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_libraries(goal_test ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} m)
target_compile_options(goal_test PRIVATE ${SDL2_CFLAGS_OTHER} -Wall -Wextra -Wno-unused-parameter)

# menu_test: SDL_VIDEODRIVER=dummy build/menu_test (from the project root)
add_executable(menu_test
    tests/menu_test.c
    src/menu.c
    src/settings.c
    src/platform.c
    src/field.c
    src/game.c
    src/pack.c
    src/assets.c
    src/text.c
)
target_include_directories(menu_test PRIVATE src ${CMAKE_SOURCE_DIR}/third_party ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_libraries(menu_test ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} m)
target_compile_options(menu_test PRIVATE ${SDL2_CFLAGS_OTHER} -Wall -Wextra -Wno-unused-parameter)
