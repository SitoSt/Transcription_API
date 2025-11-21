cmake_minimum_required(VERSION 3.16)

# Test para StreamingWhisperEngine
add_executable(test_streaming_whisper
    test_streaming_whisper.cpp
)

target_link_libraries(test_streaming_whisper
    streaming_whisper
    GTest::gtest_main
)

target_include_directories(test_streaming_whisper PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Pasar la ruta del proyecto como definición de compilación
target_compile_definitions(test_streaming_whisper PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(test_streaming_whisper)

# Test para SimpleVAD
add_executable(test_simple_vad
    test_simple_vad.cpp
)

target_link_libraries(test_simple_vad
    simple_vad
    GTest::gtest_main
)

target_include_directories(test_simple_vad PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

gtest_discover_tests(test_simple_vad)
