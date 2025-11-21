#pragma once
#include <string>
#include "PathManager.h"

/**
 * @brief Clase encargada de ejecutar whisper.cpp desde C++ y devolver la transcripción de un audio.
 */
class WhisperWrapper 
{
    public:

        static WhisperWrapper* instance(std::string model_path,  std::string whisper_binary)
        std::string transcribe(std::string);
    private:
        static WhisperWrapper* instance;
        WhisperWrapper(std::string model_path,  std::string whisper_binary);
        std::string audio_dir, model_path, whisper_binary;

        std::string readFromFile(std::string file_path);
};