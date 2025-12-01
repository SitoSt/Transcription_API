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
```

### Ejecutar el servidor

```bash
# Desde el directorio raíz
./build/transcription_server

# O especificar ruta del modelo
./build/transcription_server /path/to/model.bin
```

El servidor escuchará en `ws://localhost:9001`

### Probar con cliente Python

```bash
# Instalar dependencias
pip install websockets

# Ejecutar cliente de prueba
python clients/test_client.py test_audio.wav
```

Ver [clients/README.md](clients/README.md) para más detalles.

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
│   ├── server/
│   │   ├── StreamingSession.h          # Sesión WebSocket
│   │   └── StreamingSession.cpp
│   ├── whisper/
│   │   ├── StreamingWhisperEngine.h    # Motor de transcripción
│   │   └── StreamingWhisperEngine.cpp
│   └── server.cpp                      # Punto de entrada del servidor
├── tests/
│   ├── test_streaming_whisper.cpp      # 13 tests unitarios
│   └── CMakeLists.txt
├── third_party/
│   └── whisper.cpp/                    # Submódulo Git
│       └── models/ggml-base.bin        # Modelo de prueba
├── CMakeLists.txt
├── run_tests.sh                        # Script helper
└── README.md
```

## 🧪 Tests

El proyecto incluye **13 tests unitarios** que cubren:

### StreamingWhisperEngine (13 tests)
- ✅ Carga de modelos
- ✅ Gestión de buffer circular
- ✅ Conversión de formatos de audio
- ✅ Thread-safety
- ✅ Transcripción con diferentes tipos de audio

**Ejecutar tests**:
```bash
./run_tests.sh
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
- **Boost**: Para el servidor WebSocket (Beast & ASIO)

## 🎯 Estado del Proyecto

- [x] Integración de whisper.cpp
- [x] StreamingWhisperEngine con tests (13 tests)
- [x] Servidor WebSocket con streaming (Protocolo Mixto Text/Binary)
- [x] Cliente Python de prueba
- [ ] Cliente Web (HTML/JavaScript)
- [ ] Optimizaciones de rendimiento

**Total**: 13 tests unitarios pasando ✅

## 📡 Protocolo WebSocket

El servidor utiliza un protocolo mixto para optimizar la latencia:

1.  **Configuración (Texto/JSON)**:
    - Cliente envía: `{"type": "config", "language": "es"}`
    - Servidor responde: `{"type": "ready", ...}`

2.  **Audio (Binario)**:
    - Cliente envía: Datos crudos PCM (Float32, 16kHz, Mono).
    - Servidor acumula el audio sin responder inmediatamente.

3.  **Finalización (Texto/JSON)**:
    - Cliente envía: `{"type": "end"}` (cuando detecta silencio/fin de frase).
    - Servidor procesa todo el audio acumulado y responde: `{"type": "transcription", "text": "...", "is_final": true}`.

## 📖 Documentación

Ver [walkthrough.md](.gemini/antigravity/brain/3602ab02-543c-46e5-a974-b8e7b3e54d82/walkthrough.md) para detalles de implementación.
