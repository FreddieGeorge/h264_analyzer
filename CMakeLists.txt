cmake_minimum_required(VERSION 3.21)

project(ZStreamEye VERSION 0.1.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network OpenGL OpenGLWidgets)
find_package(PkgConfig QUIET)
include(CTest)

if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswscale
    )
endif()

if(NOT FFMPEG_FOUND)
    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_path(AVUTIL_INCLUDE_DIR libavutil/frame.h)
    find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)

    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVUTIL_LIBRARY avutil)
    find_library(SWSCALE_LIBRARY swscale)

    if(AVFORMAT_INCLUDE_DIR AND AVCODEC_INCLUDE_DIR AND AVUTIL_INCLUDE_DIR AND SWSCALE_INCLUDE_DIR
            AND AVFORMAT_LIBRARY AND AVCODEC_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        target_include_directories(FFmpeg::FFmpeg INTERFACE
            ${AVFORMAT_INCLUDE_DIR}
            ${AVCODEC_INCLUDE_DIR}
            ${AVUTIL_INCLUDE_DIR}
            ${SWSCALE_INCLUDE_DIR}
        )
        target_link_libraries(FFmpeg::FFmpeg INTERFACE
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWSCALE_LIBRARY}
        )
        set(FFMPEG_FOUND TRUE)
    endif()
endif()

if(NOT FFMPEG_FOUND)
    message(FATAL_ERROR
        "FFmpeg development libraries were not found. Install ffmpeg[avcodec,avformat,swscale] via vcpkg "
        "or install system packages such as libavformat-dev libavcodec-dev libavutil-dev libswscale-dev.")
endif()

set(ZSTREAMEYE_SOURCES
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/core/AnalysisExportWriter.h
    src/core/AnalysisExportWriter.cpp
    src/core/BitstreamParser.h
    src/core/BitstreamParser.cpp
    src/core/AacAdtsParser.h
    src/core/AacAdtsParser.cpp
    src/core/Mp3FrameParser.h
    src/core/Mp3FrameParser.cpp
    src/core/CodecKind.h
    src/core/FrameAnalysis.h
    src/core/FrameAnalysis.cpp
    src/core/MediaTypes.h
    src/core/MediaTypes.cpp
    src/core/H264FrameAnalysisAdapter.h
    src/core/H264FrameAnalysisAdapter.cpp
    src/core/HevcParser.h
    src/core/HevcParser.cpp
    src/core/RebufferState.h
    src/core/DecodeWorker.h
    src/core/DecodeWorker.cpp
    src/core/FFmpegDecoder.h
    src/core/FFmpegDecoder.cpp
    src/core/H264Parser.h
    src/core/H264Parser.cpp
    src/core/StreamDocument.h
    src/core/StreamDocument.cpp
    src/ui/FrameListView.h
    src/ui/FrameListView.cpp
    src/ui/LogDock.h
    src/ui/LogDock.cpp
    src/ui/PropertyTreeView.h
    src/ui/PropertyTreeView.cpp
    src/ui/VideoCanvas.h
    src/ui/VideoCanvas.cpp
    resources/app.qrc
)

if(WIN32)
    list(APPEND ZSTREAMEYE_SOURCES resources/windows/ZStreamEye.rc)
endif()

add_executable(ZStreamEyeApp ${ZSTREAMEYE_SOURCES})

target_include_directories(ZStreamEyeApp PRIVATE src)

target_link_libraries(ZStreamEyeApp PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(ZStreamEyeApp PRIVATE PkgConfig::FFMPEG)
else()
    target_link_libraries(ZStreamEyeApp PRIVATE FFmpeg::FFmpeg)
endif()

target_compile_features(ZStreamEyeApp PRIVATE cxx_std_17)
target_compile_definitions(ZStreamEyeApp PRIVATE
    ZSTREAMEYE_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(ZStreamEyeApp PRIVATE /utf-8)
endif()

set_target_properties(ZStreamEyeApp PROPERTIES
    OUTPUT_NAME ZStreamEyeApp
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(WIN32)
    add_executable(ZStreamEye WIN32
        src/launcher/windows_launcher.cpp
        resources/windows/ZStreamEye.rc
    )
    target_compile_features(ZStreamEye PRIVATE cxx_std_17)
    if(MINGW)
        target_link_options(ZStreamEye PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(ZStreamEyeParserTests
        tests/test_h264_parser.cpp
        src/core/AnalysisExportWriter.h
        src/core/AnalysisExportWriter.cpp
        src/core/BitstreamParser.h
        src/core/BitstreamParser.cpp
        src/core/AacAdtsParser.h
        src/core/AacAdtsParser.cpp
        src/core/Mp3FrameParser.h
        src/core/Mp3FrameParser.cpp
        src/core/CodecKind.h
        src/core/FrameAnalysis.h
        src/core/FrameAnalysis.cpp
        src/core/MediaTypes.h
        src/core/MediaTypes.cpp
        src/core/H264FrameAnalysisAdapter.h
        src/core/H264FrameAnalysisAdapter.cpp
        src/core/HevcParser.h
        src/core/HevcParser.cpp
        src/core/H264Parser.h
        src/core/H264Parser.cpp
    )
    target_include_directories(ZStreamEyeParserTests PRIVATE src)
    target_link_libraries(ZStreamEyeParserTests PRIVATE Qt6::Core)
    target_compile_definitions(ZStreamEyeParserTests PRIVATE
        H264_ANALYZER_ENABLE_TESTS
        H264_ANALYZER_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
    target_compile_features(ZStreamEyeParserTests PRIVATE cxx_std_17)
    add_test(NAME ZStreamEyeParserTests COMMAND ZStreamEyeParserTests)

    add_executable(ZStreamEyeHevcParserTests
        tests/test_hevc_parser.cpp
        src/core/BitstreamParser.h
        src/core/BitstreamParser.cpp
        src/core/CodecKind.h
        src/core/FrameAnalysis.h
        src/core/FrameAnalysis.cpp
        src/core/MediaTypes.h
        src/core/MediaTypes.cpp
        src/core/HevcParser.h
        src/core/HevcParser.cpp
    )
    target_include_directories(ZStreamEyeHevcParserTests PRIVATE src)
    target_link_libraries(ZStreamEyeHevcParserTests PRIVATE Qt6::Core)
    target_compile_features(ZStreamEyeHevcParserTests PRIVATE cxx_std_17)
    add_test(NAME ZStreamEyeHevcParserTests COMMAND ZStreamEyeHevcParserTests)

    add_executable(ZStreamEyeAacAdtsParserTests
        tests/test_aac_adts_parser.cpp
        src/core/BitstreamParser.h
        src/core/BitstreamParser.cpp
        src/core/AacAdtsParser.h
        src/core/AacAdtsParser.cpp
        src/core/CodecKind.h
        src/core/FrameAnalysis.h
        src/core/FrameAnalysis.cpp
        src/core/MediaTypes.h
        src/core/MediaTypes.cpp
    )
    target_include_directories(ZStreamEyeAacAdtsParserTests PRIVATE src)
    target_link_libraries(ZStreamEyeAacAdtsParserTests PRIVATE Qt6::Core)
    target_compile_features(ZStreamEyeAacAdtsParserTests PRIVATE cxx_std_17)
    add_test(NAME ZStreamEyeAacAdtsParserTests COMMAND ZStreamEyeAacAdtsParserTests)

    add_executable(ZStreamEyeMp3FrameParserTests
        tests/test_mp3_frame_parser.cpp
        src/core/BitstreamParser.h
        src/core/BitstreamParser.cpp
        src/core/Mp3FrameParser.h
        src/core/Mp3FrameParser.cpp
        src/core/CodecKind.h
        src/core/FrameAnalysis.h
        src/core/FrameAnalysis.cpp
        src/core/MediaTypes.h
        src/core/MediaTypes.cpp
    )
    target_include_directories(ZStreamEyeMp3FrameParserTests PRIVATE src)
    target_link_libraries(ZStreamEyeMp3FrameParserTests PRIVATE Qt6::Core)
    target_compile_features(ZStreamEyeMp3FrameParserTests PRIVATE cxx_std_17)
    add_test(NAME ZStreamEyeMp3FrameParserTests COMMAND ZStreamEyeMp3FrameParserTests)

    add_executable(ZStreamEyeRebufferStateTests
        tests/test_rebuffer_state.cpp
        src/core/RebufferState.h
    )
    target_include_directories(ZStreamEyeRebufferStateTests PRIVATE src)
    target_compile_features(ZStreamEyeRebufferStateTests PRIVATE cxx_std_17)
    add_test(NAME ZStreamEyeRebufferStateTests COMMAND ZStreamEyeRebufferStateTests)
endif()
