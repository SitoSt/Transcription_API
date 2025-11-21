#include "StreamingWhisperEngine.h"
#include <whisper.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

StreamingWhisperEngine::StreamingWhisperEngine(const std::string& model_path)
    : ctx_(nullptr), language_("es"), n_threads_(4), max_buffer_samples_(16000 * 30) {
    
    // Inicializar contexto de whisper con parámetros por defecto
    whisper_context_params cparams = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) {
        throw std::runtime_error("Failed to load whisper model: " + model_path);
    }
    
    // Pre-reservar espacio para el buffer (30 segundos @ 16kHz)
    audio_buffer_.reserve(max_buffer_samples_);
    
    std::cout << "[StreamingWhisperEngine] Modelo cargado: " << model_path << std::endl;
}

StreamingWhisperEngine::~StreamingWhisperEngine() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

void StreamingWhisperEngine::processAudioChunk(const std::vector<float>& pcm_data) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Evitar overflow del buffer
    if (audio_buffer_.size() + pcm_data.size() > max_buffer_samples_) {
        // Eliminar la primera mitad del buffer para hacer espacio
        size_t half = audio_buffer_.size() / 2;
        audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.begin() + half);
    }
    
    audio_buffer_.insert(audio_buffer_.end(), pcm_data.begin(), pcm_data.end());
}

std::string StreamingWhisperEngine::transcribe() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (audio_buffer_.empty()) {
        return "";
    }
    
    // Configurar parámetros de whisper
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = language_.c_str();
    params.n_threads = n_threads_;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_realtime = false;
    params.print_special = false;
    params.translate = false;
    params.no_context = true;
    params.single_segment = false;
    
    // Ejecutar transcripción
    int result = whisper_full(ctx_, params, audio_buffer_.data(), audio_buffer_.size());
    
    if (result != 0) {
        throw std::runtime_error("Whisper transcription failed with code: " + std::to_string(result));
    }
    
    // Extraer texto transcrito
    std::string transcription;
    const int n_segments = whisper_full_n_segments(ctx_);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        if (text) {
            transcription += text;
        }
    }
    
    return transcription;
}

void StreamingWhisperEngine::reset() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    audio_buffer_.clear();
}

size_t StreamingWhisperEngine::getBufferSize() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return audio_buffer_.size();
}

void StreamingWhisperEngine::setLanguage(const std::string& lang) {
    language_ = lang;
}

void StreamingWhisperEngine::setThreads(int n_threads) {
    if (n_threads > 0) {
        n_threads_ = n_threads;
    }
}

bool StreamingWhisperEngine::isModelLoaded() const {
    return ctx_ != nullptr;
}

std::vector<float> StreamingWhisperEngine::convertInt16ToFloat32(const std::vector<int16_t>& pcm16) {
    std::vector<float> pcm32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcm32[i] = static_cast<float>(pcm16[i]) / 32768.0f;
    }
    return pcm32;
}

std::vector<float> StreamingWhisperEngine::convertBytesToFloat32(const std::vector<uint8_t>& bytes) {
    // Convertir bytes a int16 (little-endian)
    std::vector<int16_t> pcm16(bytes.size() / 2);
    std::memcpy(pcm16.data(), bytes.data(), bytes.size());
    
    // Convertir int16 a float32
    return convertInt16ToFloat32(pcm16);
}
