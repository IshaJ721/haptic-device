cmake_minimum_required(VERSION 3.16)
project(haptic-device LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CHAI3D_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Path to the parent CHAI3D source tree")
set(CHAI3D_BUILD_DIR "${CHAI3D_ROOT}/build" CACHE PATH "Path to the parent CHAI3D build directory")
set(CHAI3D_GLFW_BUILD_DIR "${CHAI3D_BUILD_DIR}/extras/GLFW" CACHE PATH "Path to the parent GLFW package config")

if(NOT EXISTS "${CHAI3D_BUILD_DIR}/chai3dConfig.cmake")
  message(FATAL_ERROR
    "Missing CHAI3D CMake package config at ${CHAI3D_BUILD_DIR}/chai3dConfig.cmake.\n"
    "Build the parent CHAI3D tree first, for example:\n"
    "  cmake -S .. -B ../build -DCMAKE_BUILD_TYPE=Release\n"
    "  cmake --build ../build -j")
endif()

if(NOT EXISTS "${CHAI3D_GLFW_BUILD_DIR}/GLFWConfig.cmake")
  message(FATAL_ERROR
    "Missing GLFW CMake package config at ${CHAI3D_GLFW_BUILD_DIR}/GLFWConfig.cmake.\n"
    "Build the parent CHAI3D tree first so its bundled GLFW library is generated.")
endif()

find_package(Python3 REQUIRED COMPONENTS Development)
find_package(chai3d CONFIG REQUIRED PATHS "${CHAI3D_BUILD_DIR}" NO_DEFAULT_PATH)
find_package(GLFW CONFIG REQUIRED PATHS "${CHAI3D_GLFW_BUILD_DIR}" NO_DEFAULT_PATH)

set(HAPTIC_DEVICE_SOURCES
  LJ.cpp
  atom.cpp
  boundaryConditions.cpp
  inputHandling.cpp
  potentials.cpp
  utility.cpp
)

add_executable(haptic-device ${HAPTIC_DEVICE_SOURCES})

target_compile_features(haptic-device PRIVATE cxx_std_11)

target_include_directories(haptic-device PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  ${CHAI3D_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
)

target_link_directories(haptic-device PRIVATE ${CHAI3D_LIBRARY_DIRS})

target_link_libraries(haptic-device PRIVATE
  ${CHAI3D_LIBRARIES}
  ${GLFW_LIBRARIES}
  Python3::Python
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_compile_options(haptic-device PRIVATE -fno-PIE -no-pie)
  target_link_options(haptic-device PRIVATE -no-pie)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(_chai_bin_subdir "win-$<IF:$<BOOL:${CMAKE_CL_64}>,x64,Win32>")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(_chai_bin_subdir "mac-${CMAKE_HOST_SYSTEM_PROCESSOR}")
else()
  set(_chai_bin_subdir "lin-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set_target_properties(haptic-device PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CHAI3D_ROOT}/bin/${_chai_bin_subdir}"
)
