cmake_minimum_required(VERSION 3.8)
project(CLIFM)

add_compile_definitions(CLIFM_DATADIR=${CMAKE_INSTALL_PREFIX}/share)

file(GLOB SRC_FILES src/*.c)
file(GLOB HDR_FILES src/*.h)

add_executable(clifm
  ${SRC_FILES}
  ${HDR_FILES}
)

if(APPLE)
  find_package(PkgConfig REQUIRED)
  find_package(Intl REQUIRED)
  pkg_check_modules(Readline REQUIRED readline)
  pkg_check_modules(Magic REQUIRED libmagic)

  target_include_directories(clifm PUBLIC
    ${Intl_INCLUDE_DIRS}
    ${Readline_INCLUDE_DIRS}
    ${Magic_INCLUDE_DIRS})
  target_link_libraries(clifm PUBLIC
    ${Intl_LIBRARIES}
    ${Readline_LINK_LIBRARIES}
    ${Magic_LINK_LIBRARIES}
  )
elseif(UNIX)
  target_link_libraries(clifm PUBLIC
    readline
    acl
    cap
    magic
  )
endif()
