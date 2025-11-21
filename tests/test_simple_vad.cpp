#include <gtest/gtest.h>
#include "audio/SimpleVAD.h"
#include <cmath>
#include <vector>

/**
 * Test Fixture para SimpleVAD
 */
class SimpleVADTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuración por defecto
    }
    
    // Helper: Generar silencio
    std::vector<float> generateSilence(size_t num_samples) {
        return std::vector<float>(num_samples, 0.0f);
    }
    
    // Helper: Generar ruido blanco
    std::vector<float> generateWhiteNoise(size_t num_samples, float amplitude = 0.1f) {
        std::vector<float> noise(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            noise[i] = amplitude * (2.0f * static_cast<float>(rand()) / RAND_MAX - 1.0f);
        }
        return noise;
    }
    
    // Helper: Generar tono sinusoidal
    std::vector<float> generateTone(size_t num_samples, float frequency, 
                                   float sample_rate, float amplitude = 0.5f) {
        std::vector<float> tone(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            tone[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sample_rate);
        }
        return tone;
    }
    
    // Helper: Generar voz sintética (mezcla de tonos)
    std::vector<float> generateSyntheticSpeech(size_t num_samples, float sample_rate = 16000.0f) {
        auto tone1 = generateTone(num_samples, 200.0f, sample_rate, 0.3f);  // Fundamental
        auto tone2 = generateTone(num_samples, 400.0f, sample_rate, 0.2f);  // Armónico
        auto tone3 = generateTone(num_samples, 600.0f, sample_rate, 0.1f);  // Armónico
        
        std::vector<float> speech(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            speech[i] = tone1[i] + tone2[i] + tone3[i];
        }
        return speech;
    }
};

/**
 * Test 1: Constructor y valores por defecto
 */
TEST_F(SimpleVADTest, DefaultConstructor) {
    SimpleVAD vad;
    
    EXPECT_FALSE(vad.isInSpeechState());
    EXPECT_FLOAT_EQ(vad.getLastEnergy(), 0.0f);
    EXPECT_FLOAT_EQ(vad.getLastZCR(), 0.0f);
}

/**
 * Test 2: Constructor con parámetros personalizados
 */
TEST_F(SimpleVADTest, CustomConstructor) {
    SimpleVAD vad(0.05f, 5, 30);
    
    EXPECT_FALSE(vad.isInSpeechState());
}

/**
 * Test 3: Detección de silencio
 */
TEST_F(SimpleVADTest, DetectSilence) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto silence = generateSilence(1000);
    
    // Procesar varios frames de silencio
    for (int i = 0; i < 25; ++i) {
        bool is_speech = vad.isSpeech(silence);
        
        // Después de min_silence_frames, debería detectar silencio
        if (i >= 20) {
            EXPECT_FALSE(is_speech) << "Frame " << i << " debería ser silencio";
        }
    }
    
    EXPECT_FALSE(vad.isInSpeechState());
}

/**
 * Test 4: Detección de voz con tono
 */
TEST_F(SimpleVADTest, DetectSpeechWithTone) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto tone = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Procesar varios frames de tono
    for (int i = 0; i < 10; ++i) {
        bool is_speech = vad.isSpeech(tone);
        
        // Después de min_speech_frames, debería detectar voz
        if (i >= 3) {
            EXPECT_TRUE(is_speech) << "Frame " << i << " debería ser voz";
        }
    }
    
    EXPECT_TRUE(vad.isInSpeechState());
}

/**
 * Test 5: Cálculo de energía RMS
 */
TEST_F(SimpleVADTest, EnergyCalculation) {
    SimpleVAD vad;
    
    // Silencio -> energía ~0
    auto silence = generateSilence(1000);
    vad.isSpeech(silence);
    EXPECT_NEAR(vad.getLastEnergy(), 0.0f, 0.001f);
    
    // Tono de amplitud 0.5 -> energía ~0.35 (0.5/sqrt(2))
    auto tone = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    vad.isSpeech(tone);
    EXPECT_NEAR(vad.getLastEnergy(), 0.35f, 0.05f);
}

/**
 * Test 6: Cálculo de Zero Crossing Rate
 */
TEST_F(SimpleVADTest, ZeroCrossingRate) {
    SimpleVAD vad;
    
    // Silencio -> ZCR ~0
    auto silence = generateSilence(1000);
    vad.isSpeech(silence);
    EXPECT_NEAR(vad.getLastZCR(), 0.0f, 0.001f);
    
    // Tono de alta frecuencia -> ZCR alto
    auto high_tone = generateTone(1000, 4000.0f, 16000.0f, 0.5f);
    vad.isSpeech(high_tone);
    float high_zcr = vad.getLastZCR();
    
    // Tono de baja frecuencia -> ZCR bajo
    auto low_tone = generateTone(1000, 200.0f, 16000.0f, 0.5f);
    vad.isSpeech(low_tone);
    float low_zcr = vad.getLastZCR();
    
    EXPECT_GT(high_zcr, low_zcr);
}

/**
 * Test 7: Transición silencio → voz
 */
TEST_F(SimpleVADTest, SilenceToSpeechTransition) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto silence = generateSilence(1000);
    auto speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Empezar con silencio
    for (int i = 0; i < 25; ++i) {
        vad.isSpeech(silence);
    }
    EXPECT_FALSE(vad.isInSpeechState());
    
    // Transición a voz
    for (int i = 0; i < 5; ++i) {
        vad.isSpeech(speech);
    }
    EXPECT_TRUE(vad.isInSpeechState());
}

/**
 * Test 8: Transición voz → silencio
 */
TEST_F(SimpleVADTest, SpeechToSilenceTransition) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto silence = generateSilence(1000);
    auto speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Empezar con voz
    for (int i = 0; i < 10; ++i) {
        vad.isSpeech(speech);
    }
    EXPECT_TRUE(vad.isInSpeechState());
    
    // Transición a silencio
    for (int i = 0; i < 25; ++i) {
        vad.isSpeech(silence);
    }
    EXPECT_FALSE(vad.isInSpeechState());
}

/**
 * Test 9: Reset del detector
 */
TEST_F(SimpleVADTest, Reset) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Procesar voz
    for (int i = 0; i < 10; ++i) {
        vad.isSpeech(speech);
    }
    EXPECT_TRUE(vad.isInSpeechState());
    
    // Reset
    vad.reset();
    EXPECT_FALSE(vad.isInSpeechState());
    EXPECT_FLOAT_EQ(vad.getLastEnergy(), 0.0f);
    EXPECT_FLOAT_EQ(vad.getLastZCR(), 0.0f);
    
    auto stats = vad.getStats();
    EXPECT_EQ(stats.total_frames, 0);
}

/**
 * Test 10: Configuración de umbrales
 */
TEST_F(SimpleVADTest, ThresholdConfiguration) {
    SimpleVAD vad;
    
    // Umbral bajo -> detecta más voz
    vad.setEnergyThreshold(0.01f);
    auto low_noise = generateWhiteNoise(1000, 0.05f);
    for (int i = 0; i < 5; ++i) {
        vad.isSpeech(low_noise);
    }
    bool detected_with_low_threshold = vad.isInSpeechState();
    
    // Reset y umbral alto -> detecta menos voz
    vad.reset();
    vad.setEnergyThreshold(0.1f);
    for (int i = 0; i < 5; ++i) {
        vad.isSpeech(low_noise);
    }
    bool detected_with_high_threshold = vad.isInSpeechState();
    
    EXPECT_TRUE(detected_with_low_threshold);
    EXPECT_FALSE(detected_with_high_threshold);
}

/**
 * Test 11: Estadísticas del detector
 */
TEST_F(SimpleVADTest, Statistics) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto silence = generateSilence(1000);
    auto speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Procesar 10 frames de silencio
    for (int i = 0; i < 25; ++i) {
        vad.isSpeech(silence);
    }
    
    // Procesar 10 frames de voz
    for (int i = 0; i < 10; ++i) {
        vad.isSpeech(speech);
    }
    
    auto stats = vad.getStats();
    EXPECT_EQ(stats.total_frames, 35);
    EXPECT_GT(stats.speech_frames, 0);
    EXPECT_GT(stats.silence_frames, 0);
    EXPECT_GT(stats.avg_energy, 0.0f);
}

/**
 * Test 12: Buffer vacío
 */
TEST_F(SimpleVADTest, EmptyBuffer) {
    SimpleVAD vad;
    
    std::vector<float> empty;
    bool result = vad.isSpeech(empty);
    
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(vad.getLastEnergy(), 0.0f);
}

/**
 * Test 13: Histéresis (evitar flapping)
 */
TEST_F(SimpleVADTest, Hysteresis) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto silence = generateSilence(1000);
    auto weak_speech = generateTone(1000, 440.0f, 16000.0f, 0.03f);
    
    // Establecer estado de voz
    auto strong_speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    for (int i = 0; i < 5; ++i) {
        vad.isSpeech(strong_speech);
    }
    EXPECT_TRUE(vad.isInSpeechState());
    
    // Un frame débil no debería cambiar inmediatamente el estado
    vad.isSpeech(weak_speech);
    EXPECT_TRUE(vad.isInSpeechState());
    
    // Solo después de min_silence_frames de silencio cambia
    for (int i = 0; i < 19; ++i) {
        vad.isSpeech(silence);
        EXPECT_TRUE(vad.isInSpeechState()) << "Frame " << i;
    }
    
    vad.isSpeech(silence);
    EXPECT_FALSE(vad.isInSpeechState());
}

/**
 * Test 14: Voz sintética realista
 */
TEST_F(SimpleVADTest, SyntheticSpeech) {
    SimpleVAD vad(0.02f, 3, 20);
    
    auto synthetic_speech = generateSyntheticSpeech(1000);
    
    // Debería detectar como voz
    for (int i = 0; i < 10; ++i) {
        vad.isSpeech(synthetic_speech);
    }
    
    EXPECT_TRUE(vad.isInSpeechState());
    EXPECT_GT(vad.getLastEnergy(), 0.02f);
}

/**
 * Test 15: Configuración de frames mínimos
 */
TEST_F(SimpleVADTest, MinFramesConfiguration) {
    SimpleVAD vad(0.02f, 5, 30);  // Más restrictivo
    
    auto speech = generateTone(1000, 440.0f, 16000.0f, 0.5f);
    
    // Con 3 frames no debería detectar (min es 5)
    for (int i = 0; i < 3; ++i) {
        vad.isSpeech(speech);
    }
    EXPECT_FALSE(vad.isInSpeechState());
    
    // Con 5 frames sí debería detectar
    for (int i = 0; i < 3; ++i) {
        vad.isSpeech(speech);
    }
    EXPECT_TRUE(vad.isInSpeechState());
}

/**
 * Main de los tests
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
