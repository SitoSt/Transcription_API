#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * @brief Detector de Actividad de Voz (Voice Activity Detection)
 * 
 * Implementa detección de voz vs silencio usando múltiples métodos:
 * - Energía RMS (Root Mean Square)
 * - Zero Crossing Rate (ZCR)
 * - Suavizado temporal con ventana deslizante
 * 
 * Thread-safe y optimizado para streaming en tiempo real.
 */
class SimpleVAD {
public:
    /**
     * @brief Constructor
     * @param energy_threshold Umbral de energía RMS (default: 0.02)
     * @param min_speech_frames Mínimo de frames consecutivos para considerar voz (default: 3)
     * @param min_silence_frames Mínimo de frames consecutivos para considerar silencio (default: 20)
     */
    explicit SimpleVAD(
        float energy_threshold = 0.02f,
        int min_speech_frames = 3,
        int min_silence_frames = 20
    );
    
    /**
     * @brief Detectar si hay voz en el chunk de audio
     * @param pcm_data Audio en formato PCM float32, rango [-1.0, 1.0]
     * @return true si se detecta voz, false si es silencio
     */
    bool isSpeech(const std::vector<float>& pcm_data);
    
    /**
     * @brief Resetear el estado interno del detector
     */
    void reset();
    
    /**
     * @brief Obtener la energía RMS del último chunk procesado
     */
    float getLastEnergy() const { return last_energy_; }
    
    /**
     * @brief Obtener el Zero Crossing Rate del último chunk
     */
    float getLastZCR() const { return last_zcr_; }
    
    /**
     * @brief Verificar si actualmente está en estado de voz
     */
    bool isInSpeechState() const { return is_speech_state_; }
    
    /**
     * @brief Configurar umbral de energía
     */
    void setEnergyThreshold(float threshold);
    
    /**
     * @brief Configurar frames mínimos de voz
     */
    void setMinSpeechFrames(int frames);
    
    /**
     * @brief Configurar frames mínimos de silencio
     */
    void setMinSilenceFrames(int frames);
    
    /**
     * @brief Obtener estadísticas del detector
     */
    struct Stats {
        int total_frames;
        int speech_frames;
        int silence_frames;
        float avg_energy;
        float avg_zcr;
    };
    
    Stats getStats() const;

private:
    // Parámetros configurables
    float energy_threshold_;
    int min_speech_frames_;
    int min_silence_frames_;
    
    // Estado interno
    bool is_speech_state_;
    int speech_frame_count_;
    int silence_frame_count_;
    
    // Métricas del último frame
    float last_energy_;
    float last_zcr_;
    
    // Estadísticas acumuladas
    int total_frames_;
    int total_speech_frames_;
    int total_silence_frames_;
    float sum_energy_;
    float sum_zcr_;
    
    // Métodos de cálculo
    float calculateRMS(const std::vector<float>& pcm_data) const;
    float calculateZCR(const std::vector<float>& pcm_data) const;
    bool detectSpeechByEnergy(float energy) const;
};
