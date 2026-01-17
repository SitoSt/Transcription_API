#!/usr/bin/env python3
"""
Cliente de prueba para el servidor de transcripción en streaming.

Soporta múltiples modos de entrada:
1. Archivo WAV (--file)
2. Micrófono (--mic)
3. Generador de señales (--generate)
"""

import asyncio
import websockets
import json
import base64
import struct
import wave
import sys
import argparse
import math
import numpy as np
import sounddevice as sd
from pathlib import Path

class AudioGenerator:
    """Generador de señales de audio para pruebas"""
    
    @staticmethod
    def silence(duration_sec, sample_rate=16000):
        return np.zeros(int(duration_sec * sample_rate), dtype=np.float32)
    
    @staticmethod
    def white_noise(duration_sec, sample_rate=16000, amplitude=0.1):
        return np.random.uniform(-amplitude, amplitude, int(duration_sec * sample_rate)).astype(np.float32)
    
    @staticmethod
    def tone(duration_sec, freq=440, sample_rate=16000, amplitude=0.5):
        t = np.linspace(0, duration_sec, int(duration_sec * sample_rate), False)
        return (amplitude * np.sin(2 * np.pi * freq * t)).astype(np.float32)
    
    @staticmethod
    def sweep(duration_sec, start_freq=200, end_freq=2000, sample_rate=16000, amplitude=0.5):
        t = np.linspace(0, duration_sec, int(duration_sec * sample_rate), False)
        # Frecuencia instantánea lineal
        freqs = np.linspace(start_freq, end_freq, len(t))
        phases = 2 * np.pi * np.cumsum(freqs) / sample_rate
        return (amplitude * np.sin(phases)).astype(np.float32)

class TranscriptionClient:
    def __init__(self, server_url="ws://localhost:9001"):
        self.server_url = server_url
        self.ws = None
        self.running = False
    
    async def connect(self):
        """Conectar al servidor WebSocket"""
        print(f"🔌 Conectando a {self.server_url}...")
        
        ssl_context = None
        if self.server_url.startswith("wss://"):
            import ssl
            ssl_context = ssl.create_default_context()
            ssl_context.check_hostname = False
            ssl_context.verify_mode = ssl.CERT_NONE

        self.ws = await websockets.connect(
            self.server_url,
            ssl=ssl_context
        )
        print("✓ Conectado")
    
    async def configure(self, language="es", energy_threshold=0.02, min_silence_frames=20, token=None):
        """Enviar configuración inicial"""
        config_msg = {
            "type": "config",
            "language": language,
            "energy_threshold": energy_threshold,
            "min_silence_frames": min_silence_frames
        }
        
        if token:
            config_msg["token"] = token
        
        print(f"⚙️  Enviando configuración: {config_msg}")
        await self.ws.send(json.dumps(config_msg))
        
        # Esperar respuesta "ready"
        response = await self.ws.recv()
        msg = json.loads(response)
        
        if msg["type"] == "ready":
            print(f"✓ Servidor listo: {msg}")
        else:
            print(f"⚠️  Respuesta inesperada: {msg}")
    
    async def send_audio_chunk(self, float_samples, sample_rate=16000):
        """Enviar un chunk de audio (float32 array)"""
        # Convertir a bytes (float32 little-endian)
        audio_bytes = float_samples.tobytes()
        
        await self.ws.send(audio_bytes)

    async def send_audio_file(self, wav_path, chunk_duration_ms=500):
        """Enviar archivo WAV en chunks"""
        print(f"📁 Cargando audio: {wav_path}")
        
        with wave.open(str(wav_path), 'rb') as wf:
            channels = wf.getnchannels()
            sample_rate = wf.getframerate()
            sample_width = wf.getsampwidth()
            
            print(f"   Formato: {channels} canal(es), {sample_rate}Hz, {sample_width*8}bits")
            
            if sample_rate != 16000:
                print(f"⚠️  Advertencia: Se esperaba 16kHz, pero el audio es {sample_rate}Hz")
            
            chunk_samples = int(sample_rate * chunk_duration_ms / 1000)
            
            print(f"📤 Enviando audio en chunks de {chunk_duration_ms}ms...")
            self.running = True
            
            chunk_num = 0
            while self.running:
                frames = wf.readframes(chunk_samples)
                if not frames:
                    break
                
                # Convertir raw bytes a numpy float32
                if sample_width == 2: # int16
                    samples = np.frombuffer(frames, dtype=np.int16)
                    float_samples = samples.astype(np.float32) / 32768.0
                elif sample_width == 4: # float32
                    float_samples = np.frombuffer(frames, dtype=np.float32)
                else:
                    print(f"❌ Formato no soportado: {sample_width*8}bits")
                    return

                if channels > 1:
                    float_samples = float_samples[::channels] # Tomar solo primer canal
                
                await self.send_audio_chunk(float_samples, sample_rate)
                chunk_num += 1
                
                await asyncio.sleep(chunk_duration_ms / 1000.0)
                await self.process_pending_messages()

            print(f"✓ Enviados {chunk_num} chunks")

    async def send_generated_audio(self, type, duration, freq=440, chunk_duration_ms=100):
        """Enviar audio generado"""
        print(f"🎹 Generando: {type} ({duration}s)")
        
        sample_rate = 16000
        
        if type == "silence":
            audio = AudioGenerator.silence(duration, sample_rate)
        elif type == "noise":
            audio = AudioGenerator.white_noise(duration, sample_rate)
        elif type == "tone":
            audio = AudioGenerator.tone(duration, freq, sample_rate)
        elif type == "sweep":
            audio = AudioGenerator.sweep(duration, sample_rate=sample_rate)
        else:
            print(f"❌ Tipo desconocido: {type}")
            return

        chunk_samples = int(sample_rate * chunk_duration_ms / 1000)
        total_samples = len(audio)
        
        self.running = True
        for i in range(0, total_samples, chunk_samples):
            if not self.running: break
            
            chunk = audio[i:i + chunk_samples]
            await self.send_audio_chunk(chunk, sample_rate)
            
            # Simular tiempo real
            await asyncio.sleep(chunk_duration_ms / 1000.0)
            await self.process_pending_messages()

        print("✓ Generación finalizada")

    async def send_microphone_audio(self, chunk_duration_ms=100):
        """Capturar y enviar audio del micrófono"""
        print("🎤 Escuchando del micrófono... (Presiona Ctrl+C para detener)")
        
        sample_rate = 16000
        chunk_samples = int(sample_rate * chunk_duration_ms / 1000)
        queue = asyncio.Queue()
        loop = asyncio.get_event_loop()
        
        def callback(indata, frames, time, status):
            if status:
                print(status, file=sys.stderr)
            loop.call_soon_threadsafe(queue.put_nowait, indata.copy())
        
        self.running = True
        try:
            with sd.InputStream(samplerate=sample_rate, channels=1, 
                              blocksize=chunk_samples, callback=callback, dtype='float32'):
                while self.running:
                    indata = await queue.get()
                    # indata es (frames, channels), queremos flat array
                    flat_data = indata.flatten()
                    await self.send_audio_chunk(flat_data, sample_rate)
                    await self.process_pending_messages()
                    
        except KeyboardInterrupt:
            print("\n🛑 Deteniendo grabación...")
        except Exception as e:
            print(f"❌ Error de micrófono: {e}")

    async def process_pending_messages(self):
        """Procesar mensajes entrantes sin bloquear"""
        try:
            while True:
                response = await asyncio.wait_for(self.ws.recv(), timeout=0.001)
                await self.handle_message(response)
        except (asyncio.TimeoutError, websockets.exceptions.ConnectionClosed):
            pass
    
    async def send_end(self):
        """Enviar mensaje de finalización"""
        end_msg = {"type": "end"}
        await self.ws.send(json.dumps(end_msg))
        print("🏁 Fin de transmisión enviado")
    
    async def handle_message(self, message):
        """Manejar mensaje del servidor"""
        try:
            msg = json.loads(message)
            msg_type = msg.get("type")
            
            if msg_type == "transcription":
                is_final = msg.get("is_final", False)
                text = msg.get("text", "")
                marker = "🔴" if is_final else "⚪"
                print(f"{marker} {text}")
            
            elif msg_type == "vad_state":
                is_speech = msg.get("is_speech", False)
                # energy = msg.get("energy", 0.0)
                # state = "🎤" if is_speech else "🔇"
                # print(f"{state}", end="\r", flush=True) 
                pass # Reducir ruido en consola
            
            elif msg_type == "error":
                print(f"❌ Error servidor: {msg.get('message')}")
            
        except Exception as e:
            print(f"Error parsing msg: {e}")
    
    async def close(self):
        """Cerrar conexión"""
        self.running = False
        if self.ws:
            await self.ws.close()
            print("👋 Desconectado")


async def main():
    parser = argparse.ArgumentParser(description="Cliente de Test para Transcripción")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--file", type=str, help="Archivo WAV a enviar")
    group.add_argument("--mic", action="store_true", help="Usar micrófono")
    group.add_argument("--generate", choices=["silence", "noise", "tone", "sweep"], help="Generar audio sintético")
    
    parser.add_argument("--duration", type=float, default=5.0, help="Duración para generación (segundos)")
    parser.add_argument("--freq", type=float, default=440.0, help="Frecuencia para tono (Hz)")
    parser.add_argument("--url", type=str, default="ws://localhost:9001", help="URL del servidor")
    parser.add_argument("--token", type=str, help="Token de autenticación")
    
    args = parser.parse_args()
    
    client = TranscriptionClient(server_url=args.url)
    
    try:
        await client.connect()
        await client.configure(token=args.token)
        
        if args.file:
            if not Path(args.file).exists():
                print(f"❌ Archivo no encontrado: {args.file}")
                return
            await client.send_audio_file(args.file)
            
        elif args.mic:
            await client.send_microphone_audio()
            
        elif args.generate:
            await client.send_generated_audio(args.generate, args.duration, args.freq)
            
        await client.send_end()
        
        # Esperar últimos mensajes
        print("⏳ Esperando respuestas finales (3s)...")
        t_end = asyncio.get_event_loop().time() + 3.0
        while asyncio.get_event_loop().time() < t_end:
            await client.process_pending_messages()
            await asyncio.sleep(0.1)
            
    except KeyboardInterrupt:
        print("\n👋 Interrumpido por usuario")
    except Exception as e:
        print(f"❌ Error fatal: {e}")
    finally:
        await client.close()

if __name__ == "__main__":
    asyncio.run(main())
