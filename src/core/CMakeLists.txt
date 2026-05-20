set(ZSTREAMEYE_CORE_SOURCES
    buffering/RebufferState.h
    decode/DecodeWorker.h
    decode/DecodeWorker.cpp
    decode/FFmpegDecoder.h
    decode/FFmpegDecoder.cpp
    export/AnalysisExportWriter.h
    export/AnalysisExportWriter.cpp
    model/CodecKind.h
    model/FrameAnalysis.h
    model/FrameAnalysis.cpp
    model/MediaTypes.h
    model/MediaTypes.cpp
    model/StreamDocument.h
    model/StreamDocument.cpp
    parser/BitstreamParser.h
    parser/BitstreamParser.cpp
    parser/audio/AacAdtsParser.h
    parser/audio/AacAdtsParser.cpp
    parser/audio/Mp3FrameParser.h
    parser/audio/Mp3FrameParser.cpp
    parser/video/h264/H264FrameAnalysisAdapter.h
    parser/video/h264/H264FrameAnalysisAdapter.cpp
    parser/video/h264/H264MacroblockParser.cpp
    parser/video/h264/H264ParameterSetParser.cpp
    parser/video/h264/H264Parser.h
    parser/video/h264/H264Parser.cpp
    parser/video/h264/H264SliceHeaderParser.cpp
    parser/video/hevc/HevcParser.h
    parser/video/hevc/HevcParser.cpp
    util/BitReader.h
    util/BitReader.cpp
    util/ByteStream.h
    util/ByteStream.cpp
    util/Rbsp.h
    util/Rbsp.cpp
)

add_library(ZStreamEyeCore STATIC ${ZSTREAMEYE_CORE_SOURCES})

target_include_directories(ZStreamEyeCore PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ZStreamEyeCore PUBLIC
    Qt6::Core
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(ZStreamEyeCore PUBLIC PkgConfig::FFMPEG)
else()
    target_link_libraries(ZStreamEyeCore PUBLIC FFmpeg::FFmpeg)
endif()

target_compile_features(ZStreamEyeCore PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(ZStreamEyeCore PRIVATE /utf-8)
endif()
