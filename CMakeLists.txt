cmake_minimum_required(VERSION 3.19)
project(P5Generator LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# macOS deployment target
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum OS X deployment version")
    add_compile_options(-fobjc-arc)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/openfx
)

# Source files for plugin
set(PLUGIN_SOURCES
    src/P5GeneratorPlugin.cpp
    src/P5WebKitBridge.mm
)

# OpenFX Module (macOS Shared Library Bundle)
add_library(P5Generator MODULE ${PLUGIN_SOURCES})

# Target properties for OpenFX Bundle structure
set_target_properties(P5Generator PROPERTIES
    PREFIX ""
    OUTPUT_NAME "P5Generator.ofx"
    SUFFIX ""
)

target_link_libraries(P5Generator PRIVATE
    "-framework Cocoa"
    "-framework WebKit"
    "-framework IOSurface"
    "-framework Metal"
    "-framework CoreGraphics"
)

# Custom command to assemble the .ofx.bundle structure
set(BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/P5Generator.ofx.bundle")
set(BUNDLE_MACOS_DIR "${BUNDLE_DIR}/Contents/MacOS")
set(BUNDLE_RESOURCES_DIR "${BUNDLE_DIR}/Contents/Resources")

add_custom_command(TARGET P5Generator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_MACOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:P5Generator>" "${BUNDLE_MACOS_DIR}/P5Generator.ofx"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" "${BUNDLE_DIR}/Contents/Info.plist"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/index.html" "${BUNDLE_RESOURCES_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/p5.min.js" "${BUNDLE_RESOURCES_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/p5_shim.js" "${BUNDLE_RESOURCES_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/default_sketch.js" "${BUNDLE_RESOURCES_DIR}/"
    COMMENT "Assembling P5Generator.ofx.bundle with Resources and Info.plist"
)

# Standalone Test Harness executable
add_executable(test_p5_bridge
    src/main_test.mm
    src/P5WebKitBridge.mm
)

target_link_libraries(test_p5_bridge PRIVATE
    "-framework Cocoa"
    "-framework WebKit"
    "-framework IOSurface"
    "-framework Metal"
    "-framework CoreGraphics"
)

# Installation targets
if(NOT DEFINED OFX_INSTALL_DIR)
    set(OFX_INSTALL_DIR "/Library/OFX/Plugins")
endif()

install(DIRECTORY "${BUNDLE_DIR}"
    DESTINATION "${OFX_INSTALL_DIR}"
    USE_SOURCE_PERMISSIONS
)
