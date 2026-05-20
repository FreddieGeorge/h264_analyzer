function(add_zstreameye_core_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ZStreamEyeCore)
    target_compile_features(${target} PRIVATE cxx_std_17)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_zstreameye_core_test(ZStreamEyeParserTests test_h264_parser.cpp)
target_compile_definitions(ZStreamEyeParserTests PRIVATE
    H264_ANALYZER_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_zstreameye_core_test(ZStreamEyeHevcParserTests test_hevc_parser.cpp)
add_zstreameye_core_test(ZStreamEyeAacAdtsParserTests test_aac_adts_parser.cpp)
add_zstreameye_core_test(ZStreamEyeMp3FrameParserTests test_mp3_frame_parser.cpp)
add_zstreameye_core_test(ZStreamEyeRebufferStateTests test_rebuffer_state.cpp)
add_zstreameye_core_test(ZStreamEyeUtilTests test_util.cpp)
add_zstreameye_core_test(ZStreamEyeAnalysisStatsTests test_analysis_stats.cpp)
