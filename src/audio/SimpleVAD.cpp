#include "SimpleVAD.h"
#include <numeric>
#include <cmath>

SimpleVAD::SimpleVAD(float energy_threshold, int min_speech_frames, int min_silence_frames)
    : energy_threshold_(energy_threshold),
      min_speech_frames_(min_speech_frames),
      min_silence_frames_(min_silence_frames),
      is_speech_state_(false),
      speech_frame_count_(0),
      silence_frame_count_(0),
      last_energy_(0.0f),
      last_zcr_(0.0f),
      total_frames_(0),
      total_speech_frames_(0),
      total_silence_frames_(0),
      sum_energy_(0.0f),
      sum_zcr_(0.0f) {
}

bool SimpleVAD::isSpeech(const std::vector<float>& pcm_data) {
    if (pcm_data.empty()) {
        return false;
    }
    
    // Calcular métricas del frame actual
    last_energy_ = calculateRMS(pcm_data);
    last_zcr_ = calculateZCR(pcm_data);
    
    // Actualizar estadísticas
    total_frames_++;
    sum_energy_ += last_energy_;
    sum_zcr_ += last_zcr_;
    
    // Detectar voz basado en energía
    bool has_speech = detectSpeechByEnergy(last_energy_);
    
    // Máquina de estados con histéresis
    if (has_speech) {
        speech_frame_count_++;
        silence_frame_count_ = 0;
        
        // Transición a estado de voz si se supera el mínimo
        if (speech_frame_count_ >= min_speech_frames_) {
            is_speech_state_ = true;
        }
    } else {
        silence_frame_count_++;
        speech_frame_count_ = 0;
        
        // Transición a estado de silencio si se supera el mínimo
        if (silence_frame_count_ >= min_silence_frames_) {
            is_speech_state_ = false;
        }
    }
    
    // Actualizar contadores de estadísticas
    if (is_speech_state_) {
        total_speech_frames_++;
    } else {
        total_silence_frames_++;
    }
    
    return is_speech_state_;
}

void SimpleVAD::reset() {
    is_speech_state_ = false;
    speech_frame_count_ = 0;
    silence_frame_count_ = 0;
    last_energy_ = 0.0f;
    last_zcr_ = 0.0f;
    total_frames_ = 0;
    total_speech_frames_ = 0;
    total_silence_frames_ = 0;
    sum_energy_ = 0.0f;
    sum_zcr_ = 0.0f;
}

void SimpleVAD::setEnergyThreshold(float threshold) {
    if (threshold >= 0.0f && threshold <= 1.0f) {
        energy_threshold_ = threshold;
    }
}

void SimpleVAD::setMinSpeechFrames(int frames) {
    if (frames > 0) {
        min_speech_frames_ = frames;
    }
}

void SimpleVAD::setMinSilenceFrames(int frames) {
    if (frames > 0) {
        min_silence_frames_ = frames;
    }
}

SimpleVAD::Stats SimpleVAD::getStats() const {
    Stats stats;
    stats.total_frames = total_frames_;
    stats.speech_frames = total_speech_frames_;
    stats.silence_frames = total_silence_frames_;
    stats.avg_energy = total_frames_ > 0 ? sum_energy_ / total_frames_ : 0.0f;
    stats.avg_zcr = total_frames_ > 0 ? sum_zcr_ / total_frames_ : 0.0f;
    return stats;
}

float SimpleVAD::calculateRMS(const std::vector<float>& pcm_data) const {
    if (pcm_data.empty()) {
        return 0.0f;
    }
    
    // Calcular suma de cuadrados
    float sum_squares = 0.0f;
    for (float sample : pcm_data) {
        sum_squares += sample * sample;
    }
    
    // RMS = sqrt(mean(x^2))
    return std::sqrt(sum_squares / pcm_data.size());
}

float SimpleVAD::calculateZCR(const std::vector<float>& pcm_data) const {
    if (pcm_data.size() < 2) {
        return 0.0f;
    }
    
    // Contar cruces por cero
    int zero_crossings = 0;
    for (size_t i = 1; i < pcm_data.size(); ++i) {
        // Detectar cambio de signo
        if ((pcm_data[i] >= 0.0f && pcm_data[i-1] < 0.0f) ||
            (pcm_data[i] < 0.0f && pcm_data[i-1] >= 0.0f)) {
            zero_crossings++;
        }
    }
    
    // Normalizar por el número de muestras
    return static_cast<float>(zero_crossings) / (pcm_data.size() - 1);
}

bool SimpleVAD::detectSpeechByEnergy(float energy) const {
    return energy > energy_threshold_;
}
