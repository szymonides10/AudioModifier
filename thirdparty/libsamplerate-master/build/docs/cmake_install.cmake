# Install script for directory: C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/libsamplerate")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/libsamplerate" TYPE FILE FILES
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/api.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/api_callback.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/api_full.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/api_misc.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/api_simple.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/bugs.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/download.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/faq.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/history.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/index.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/license.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/lists.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/quality.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/win32.md"
    "C:/Users/Szymon/QtProjects/AudioModifier/thirdparty/libsamplerate-master/docs/SRC.png"
    )
endif()

