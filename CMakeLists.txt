project(pvr.iptvsimple)

cmake_minimum_required(VERSION 2.6)

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${ZLIB_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_SYSTEM_INCLUDE_PATH /include "${CMAKE_INSTALL_PREFIX}/include")

set(DEPLIBS ${p8-platform_LIBRARIES}
            ${ZLIB_LIBRARIES})

message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")

set(IPTV_SOURCES src/client.cpp
                 src/PVRIptvData.cpp
                 src/PVRDvrData.cpp
                 src/PVRPlayList.cpp
                 src/PVRUtils.cpp
                 src/PVRSchedulerThread.cpp
                 src/PVRRecorderThread.cpp
                 src/PVRReaderThread.cpp
                 src/libexecstream/exec-stream.cpp)

build_addon(pvr.iptvsimple IPTV DEPLIBS)

include(CPack)
