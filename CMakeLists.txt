cmake_minimum_required(VERSION 3.8)
project(CLIFM)

file(GLOB SRC_FILES src/*.c)
file(GLOB HDR_FILES src/*.h)

add_executable(clifm
  ${SRC_FILES}
  ${HDR_FILES}
)
target_link_libraries(clifm PUBLIC
  readline
  acl
  cap
  magic
)
