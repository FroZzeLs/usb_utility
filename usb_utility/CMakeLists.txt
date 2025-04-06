cmake_minimum_required(VERSION 3.10) # Minimum version for modern CMake features

project(UsbUtility VERSION 1.0 LANGUAGES CXX)

# Set C++ standard (C++17 needed for std::optional, std::filesystem if used)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Avoid GNU extensions if possible

# Find PkgConfig to locate libraries
find_package(PkgConfig REQUIRED)

# Find libusb-1.0 using PkgConfig
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
if(NOT LIBUSB_FOUND)
    message(FATAL_ERROR "libusb-1.0 not found. Please install libusb-1.0-0-dev.")
endif()

# Find libudev using PkgConfig
pkg_check_modules(LIBUDEV REQUIRED libudev)
if(NOT LIBUDEV_FOUND)
    message(FATAL_ERROR "libudev not found. Please install libudev-dev.")
endif()

# Add executable target
add_executable(usb_utility
    src/main.cpp
    src/logger.cpp
    src/usb_lister.cpp
    src/usb_detector.cpp
    # utils.h is header-only, no need to list here
)

# Add include directories provided by PkgConfig
target_include_directories(usb_utility PRIVATE
    ${LIBUSB_INCLUDE_DIRS}
    ${LIBUDEV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src # Include our own src dir for headers
)

# Link libraries provided by PkgConfig
target_link_libraries(usb_utility PRIVATE
    ${LIBUSB_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
)

# Optional: Add compiler warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    target_compile_options(usb_utility PRIVATE -Wall -Wextra -pedantic)
endif()