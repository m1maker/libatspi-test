cmake_minimum_required (VERSION 3.25)

project(libatspi-test LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(ATSPI REQUIRED atspi-2)

pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_executable(${PROJECT_NAME}-listener listener.c)
add_executable(${PROJECT_NAME}-announcer announcer.c)

target_include_directories(${PROJECT_NAME}-listener PRIVATE
    ${ATSPI_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}-listener PRIVATE
    ${ATSPI_LIBRARIES}
    ${GOBJECT_LIBRARIES}
)
