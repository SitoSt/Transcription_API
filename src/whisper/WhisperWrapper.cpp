#include "DataManager.h"
#include "DataTypes.h"
#include "FileExceptions.h"
#include "PathManager.h"
#include "WhisperExceptions.h"
#include "WhisperWrapper.h"
#include <iostream>
#include <sstream>
#include <memory>
#include <array>
#include <fstream>
using namespace std;

WhisperWrapper::WhisperWrapper(string model_path_, string whisper_binary_) 
: model_path(model_path_), whisper_binary(whisper_binary_)
{
    audio_dir = DataManager::getDataPath(DataType::AUDIO_RECORDING);
    if (!filesystem::exists(model_path))
       throw JotaException(ErrorCode::WHISPER_MODEL_NOT_FOUND, "model path not found", model_path); 

    if (!filesystem::exists(whisper_binary)) 
        throw WhisperExecutionException(whisper_binary + " not found", -1);
}

WhisperWrapper* WhisperWrapper::instance(string model_path,  string whisper_binary){
    if(!instance)
    {
        instance = new WhisperWrapper(model_path, whisper_binary)
    }
    return instance;
}

/**
 * @brief Clase encargada de ejecutar whisper.cpp y devolver la transcripción de un audio.
 *
 * Esta clase encapsula la -llamada al binario whisper-cli, pasando un archivo .wav como entrada,
 * y recuperando el resultado transcrito desde un archivo .txt generado automáticamente.
 *
 * Requisitos:
 * - El binario whisper-cli debe existir en ./build/bin/whisper-cli
 * - El modelo .bin debe estar disponible (ej: models/ggml-base.bin)
 * - El archivo .wav a transcribir debe existir y estar en formato PCM 16 bits mono
 * - El programa debe tener permisos para ejecutar comandos del sistema y leer archivos de texto
 * 
 * @param session_id Identificador de sesión para reconocer la transcripcon con una marca de tiempo.
 */
string WhisperWrapper::transcribe(string session_id) 
{  
    string audio_path = audio_dir + "/" + session_id + ".wav";
    string outputPath = DataManager::getDataPath(DataType::TRANSCRIPT) / session_id;
    string command;
    int exitCode;

    if (!filesystem::exists(audio_path)) 
        throw FileNotFoundException(audio_path);

    command = whisper_binary + " -m " + model_path
        + " -f " + audio_path 
        + " -l es --output-txt -of " + outputPath
        + " -np > /dev/null";
    exitCode = system(command.c_str());
    if (exitCode != 0) 
        throw WhisperExecutionException(command, exitCode);
    
    return readFromFile(outputPath + ".txt");
}

string WhisperWrapper::readFromFile(string file_path) 
{
    stringstream buffer;
    ifstream file;

    file.open(file_path);
    if (!file) 
        throw FileNotFoundException(file_path);
    
    buffer << file.rdbuf();
    file.close();

    if (buffer.str().empty()) {
        throw WhisperException(ErrorCode::WHISPER_OUTPUT_NOT_FOUND, 
                              "Whisper generated empty transcription");
    }
    
    return buffer.str();
}