#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Forward declaration para evitar incluir whisper.h en el header
struct whisper_context;

/**
 * @brief Motor de transcripción en streaming usando whisper.cpp
 * 
 * Esta clase encapsula la API C de whisper.cpp para procesar audio en streaming.
 * Permite acumular chunks de audio y transcribirlos cuando sea necesario.
 * 
 * Thread-safe: Puede ser usado desde múltiples threads.
 */
class StreamingWhisperEngine {
public:
    /**
     * @brief Constructor
     * @param model_path Ruta al modelo .bin de whisper
     * @throws std::runtime_error si el modelo no se puede cargar
     */
    explicit StreamingWhisperEngine(const std::string& model_path);
    
    /**
     * @brief Destructor - libera recursos de whisper
     */
    ~StreamingWhisperEngine();
    
    // No copiable
    StreamingWhisperEngine(const StreamingWhisperEngine&) = delete;
    StreamingWhisperEngine& operator=(const StreamingWhisperEngine&) = delete;
    
    /**
     * @brief Agregar chunk de audio al buffer
     * @param pcm_data Audio en formato PCM float32, rango [-1.0, 1.0], 16kHz mono
     */
    void processAudioChunk(const std::vector<float>& pcm_data);
    
    /**
     * @brief Transcribir el buffer acumulado
     * @return Texto transcrito
     * @throws std::runtime_error si la transcripción falla
     */
    std::string transcribe();
    
    /**
     * @brief Limpiar el buffer de audio
     */
    void reset();
    
    /**
     * @brief Obtener tamaño actual del buffer en samples
     */
    size_t getBufferSize() const;
    
    /**
     * @brief Configurar idioma de transcripción
     * @param lang Código de idioma (ej: "es", "en", "auto")
     */
    void setLanguage(const std::string& lang);
    
    /**
     * @brief Configurar número de threads para transcripción
     * @param n_threads Número de threads (default: 4)
     */
    void setThreads(int n_threads);
    
    /**
     * @brief Verificar si el modelo está cargado correctamente
     */
    bool isModelLoaded() const;
    
    /**
     * @brief Convertir audio PCM int16 a float32
     * @param pcm16 Audio en formato int16 [-32768, 32767]
     * @return Audio en formato float32 [-1.0, 1.0]
     */
    static std::vector<float> convertInt16ToFloat32(const std::vector<int16_t>& pcm16);
    
    /**
     * @brief Convertir bytes raw a PCM float32
     * @param bytes Bytes raw (little-endian int16)
     * @return Audio en formato float32
     */
    static std::vector<float> convertBytesToFloat32(const std::vector<uint8_t>& bytes);

private:
    whisper_context* ctx_;
    std::vector<float> audio_buffer_;
    mutable std::mutex buffer_mutex_;
    
    // Parámetros de configuración
    std::string language_;
    int n_threads_;
    int max_buffer_samples_; // Máximo de samples en buffer (30 segundos @ 16kHz)
};
