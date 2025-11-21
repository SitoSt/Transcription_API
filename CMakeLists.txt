cmake_minimum_required(VERSION 3.16)
project(transcription_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opciones
option(BUILD_TESTS "Build tests" ON)

# Agregar whisper.cpp como subdirectorio
add_subdirectory(third_party/whisper.cpp)

# Encontrar dependencias
find_package(Boost REQUIRED)

# Librería StreamingWhisperEngine
add_library(streaming_whisper
    src/whisper/StreamingWhisperEngine.cpp
)

target_include_directories(streaming_whisper PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/include
)

target_link_libraries(streaming_whisper PUBLIC
    whisper
)

# Librería SimpleVAD
add_library(simple_vad
    src/audio/SimpleVAD.cpp
)

target_include_directories(simple_vad PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Servidor principal (comentado por ahora, lo actualizaremos después)
# add_executable(transcription_server
#     src/server.cpp
# )
# 
# target_include_directories(transcription_server PRIVATE
#     src
# )
# 
# target_link_libraries(transcription_server PRIVATE 
#     Boost::boost 
#     pthread
#     streaming_whisper
#     simple_vad
# )

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    # Descargar Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Para Windows: Prevenir sobrescribir las flags del proyecto padre
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    add_subdirectory(tests)
endif()