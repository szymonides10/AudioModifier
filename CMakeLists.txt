cmake_minimum_required(VERSION 3.19)
project(AudioModifier LANGUAGES CXX)

# Find Qt
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)

qt_standard_project_setup()
qt_add_resources(RESOURCE_FILES resources.qrc)

# Set paths for third-party libraries
set(SOUNDTOUCH_DIR "${CMAKE_SOURCE_DIR}/thirdparty/soundtouch-master")
set(LIBSAMPLERATE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/libsamplerate-master")

# Include headers
include_directories(${SOUNDTOUCH_DIR}/include)
include_directories(${LIBSAMPLERATE_DIR}/include)

add_library(SoundTouch STATIC
    ${SOUNDTOUCH_DIR}/source/SoundTouch/AAFilter.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/BPMDetect.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/cpu_detect_x86.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/FIFOSampleBuffer.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/FIRFilter.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/InterpolateCubic.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/InterpolateLinear.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/InterpolateShannon.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/mmx_optimized.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/PeakFinder.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/RateTransposer.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/SoundTouch.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/sse_optimized.cpp
    ${SOUNDTOUCH_DIR}/source/SoundTouch/TDStretch.cpp
)
target_include_directories(SoundTouch PUBLIC ${SOUNDTOUCH_DIR}/include)
target_compile_definitions(SoundTouch PRIVATE SOUNDTOUCHDLL)

add_subdirectory(${LIBSAMPLERATE_DIR})

qt_add_executable(AudioModifier
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    audiothread.h
    audiothread.cpp
    ${RESOURCE_FILES}
    levelmeter.h
    levelmeter.cpp
)

# Link libraries
target_link_libraries(AudioModifier
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
        SoundTouch
        samplerate
)

# Install rules
include(GNUInstallDirs)
install(TARGETS AudioModifier
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET AudioModifier
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
