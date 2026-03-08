cmake_minimum_required(VERSION 3.16)

add_executable(unit_tests
    unit/test_hallucination_guard.cpp
    unit/test_audio_pipeline.cpp
    unit/test_inference_limiter.cpp
    unit/test_connection_limiter.cpp
    unit/test_session_tracker.cpp
    unit/test_model_cache.cpp
    unit/test_streaming_whisper_engine.cpp
    # Fuentes del servidor que no tienen dependencias de Boost/OpenSSL
    ${CMAKE_SOURCE_DIR}/src/server/ConnectionLimiter.cpp
    ${CMAKE_SOURCE_DIR}/src/server/ConnectionGuard.cpp
)

target_link_libraries(unit_tests
    streaming_whisper
    GTest::gtest_main
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(unit_tests PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
