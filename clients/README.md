# Clientes de Prueba

Este directorio contiene clientes de ejemplo para probar el servidor de transcripción en streaming.

## Cliente Python

### Requisitos

```bash
pip install websockets numpy sounddevice
```

### Uso

El cliente soporta tres modos de operación:

#### 1. Archivo de Audio
Envía un archivo WAV existente:
```bash
python test_client.py --file <archivo.wav>
```

#### 2. Micrófono
Transmite audio en tiempo real desde el micrófono predeterminado:
```bash
python test_client.py --mic
```

#### 3. Generación de Audio
Genera señales de prueba para verificar robustez y VAD:
```bash
# Silencio (Testear VAD)
python test_client.py --generate silence --duration 5

# Ruido Blanco (Testear Robustez)
python test_client.py --generate noise --duration 5

# Tono Puro (Testear Procesamiento de Señal)
python test_client.py --generate tone --freq 440
```

### Opciones Comunes

- `--url`: URL del servidor (default: `ws://localhost:9001`)
- `--duration`: Duración en segundos para generación (default: 5.0)
- `--freq`: Frecuencia en Hz para tonos (default: 440.0)

### Formato de Audio

El cliente acepta archivos WAV con las siguientes características:
- **Canales**: Mono (1 canal) - si es estéreo, usará solo el primer canal
- **Sample Rate**: 16kHz (recomendado) - otros rates funcionan pero pueden afectar la precisión
- **Formato**: int16 o float32

### Convertir Audio con FFmpeg

Si tienes un archivo de audio en otro formato, puedes convertirlo con FFmpeg:

```bash
# Convertir cualquier audio a WAV 16kHz mono
ffmpeg -i input.mp3 -ar 16000 -ac 1 -sample_fmt s16 output.wav

# Desde un video
ffmpeg -i video.mp4 -ar 16000 -ac 1 -sample_fmt s16 audio.wav
```

### Protocolo

El cliente implementa el protocolo JSON del servidor:

1. **Configuración**:
```json
{
  "type": "config",
  "language": "es",
  "energy_threshold": 0.02,
  "min_silence_frames": 20
}
```

2. **Audio** (chunks de 100ms):
```json
{
  "type": "audio",
  "data": "base64_encoded_float32_pcm",
  "sample_rate": 16000,
  "channels": 1
}
```

3. **Finalización**:
```json
{
  "type": "end"
}
```

### Respuestas del Servidor

- **Ready**: Confirmación de configuración
- **Transcription**: Texto transcrito (parcial o final)
- **VAD State**: Estado de detección de voz (opcional)
- **Error**: Mensajes de error

## Cliente Web (Próximamente)

Cliente HTML/JavaScript para capturar audio del micrófono en tiempo real.

## Ejemplo de Salida

```
🔌 Conectando a ws://localhost:9001...
✓ Conectado
⚙️  Enviando configuración: {'type': 'config', 'language': 'es', ...}
✓ Servidor listo: {'type': 'ready', 'session_id': 'session-1234567890-5678', ...}
📁 Cargando audio: test.wav
   Formato: 1 canal(es), 16000Hz, 16bits
📤 Enviando audio en chunks de 100ms (1600 samples)...
🔴 Transcripción: Hola mundo
🔴 Transcripción: Cómo estás
✓ Enviados 50 chunks
🏁 Mensaje de finalización enviado
👋 Desconectado
```
