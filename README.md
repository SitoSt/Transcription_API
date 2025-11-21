# Microservicio de Transcripción en Streaming - C++

Sistema de transcripción de audio en tiempo real usando whisper.cpp y WebSocket.

## 🚀 Quick Start

### Compilar el proyecto
```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j
```

### Ejecutar tests
```bash
./run_tests.sh
```

O manualmente:
```bash
cd build/tests
./test_streaming_whisper  # 13 tests
./test_simple_vad          # 15 tests
```

### Instalación en Ubuntu/Linux

Si estás en un servidor Ubuntu, primero instala las dependencias:

```bash
# Dependencias básicas
sudo apt update
sudo apt install -y build-essential cmake git

# Opcional: OpenBLAS para mejor rendimiento en CPU
sudo apt install -y libopenblas-dev

# Opcional: CUDA para aceleración GPU (si tienes NVIDIA)
# sudo apt install -y nvidia-cuda-toolkit
```

Luego compila normalmente:
```bash
git clone --recursive https://github.com/tu-usuario/transcription.git
cd transcription
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
./run_tests.sh
```


## 📁 Estructura del Proyecto

```
transcription/
├── src/
│   ├── whisper/
│   │   ├── StreamingWhisperEngine.h    # Motor de transcripción
│   │   └── StreamingWhisperEngine.cpp
│   └── audio/
│       ├── SimpleVAD.h                 # Detección de voz
│       └── SimpleVAD.cpp
├── tests/
│   ├── test_streaming_whisper.cpp      # 13 tests unitarios
│   ├── test_simple_vad.cpp             # 15 tests unitarios
│   └── CMakeLists.txt
├── third_party/
│   └── whisper.cpp/                    # Submódulo Git
│       └── models/ggml-base.bin        # Modelo de prueba
├── CMakeLists.txt
├── run_tests.sh                        # Script helper
└── README.md
```

## 🧪 Tests

El proyecto incluye **28 tests unitarios** que cubren:

### StreamingWhisperEngine (13 tests)
- ✅ Carga de modelos
- ✅ Gestión de buffer circular
- ✅ Conversión de formatos de audio
- ✅ Thread-safety
- ✅ Transcripción con diferentes tipos de audio

### SimpleVAD (15 tests)
- ✅ Detección de silencio y voz
- ✅ Cálculo de energía RMS
- ✅ Zero Crossing Rate (ZCR)
- ✅ Transiciones de estado
- ✅ Histéresis (anti-flapping)
- ✅ Configuración de umbrales

**Ejecutar tests específicos**:
```bash
./run_tests.sh --gtest_filter=SimpleVADTest.Hysteresis
```

## 📝 Uso

### StreamingWhisperEngine

```cpp
#include "whisper/StreamingWhisperEngine.h"

// Crear motor con modelo
StreamingWhisperEngine engine("path/to/model.bin");
engine.setLanguage("es");
engine.setThreads(4);

// Procesar chunks de audio (PCM float32, 16kHz mono)
std::vector<float> audio_chunk = /* ... */;
engine.processAudioChunk(audio_chunk);

// Transcribir cuando sea necesario
std::string transcription = engine.transcribe();

// Limpiar buffer
engine.reset();
```

### SimpleVAD

```cpp
#include "audio/SimpleVAD.h"

// Crear detector con umbrales personalizados
SimpleVAD vad(
    0.02f,  // Umbral de energía
    3,      // Frames mínimos de voz
    20      // Frames mínimos de silencio
);

// Procesar chunks de audio
std::vector<float> audio_chunk = /* ... */;
bool is_speech = vad.isSpeech(audio_chunk);

if (!is_speech) {
    // Silencio detectado → transcribir buffer acumulado
    std::string text = engine.transcribe();
    engine.reset();
    vad.reset();
}
```

## 🔧 Requisitos

- CMake 3.16+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Sistema operativo: **Linux** (Ubuntu, Debian, etc.), macOS, o Windows

> **Nota**: Este proyecto es **multiplataforma** y está diseñado para funcionar en servidores Ubuntu/Linux. Los tests se han ejecutado en macOS con Apple Silicon, pero whisper.cpp soporta todas las plataformas.

## 📦 Dependencias

- **whisper.cpp**: Submódulo Git (se descarga automáticamente)
  - En Linux: usa CPU, OpenBLAS, o CUDA (GPU NVIDIA)
  - En macOS: usa Metal (Apple Silicon) o Accelerate Framework
  - En Windows: usa CPU o CUDA
- **Google Test**: Se descarga automáticamente vía FetchContent
- **Boost** (opcional): Para el servidor WebSocket (próximamente)

## 🎯 Estado del Proyecto

- [x] Integración de whisper.cpp
- [x] StreamingWhisperEngine con tests
- [x] VAD (Voice Activity Detection) con tests
- [ ] Servidor WebSocket con streaming
- [ ] Clientes de prueba (Python/Web)

## 📖 Documentación

Ver [walkthrough.md](.gemini/antigravity/brain/3602ab02-543c-46e5-a974-b8e7b3e54d82/walkthrough.md) para detalles de implementación.
