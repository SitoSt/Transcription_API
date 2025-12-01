#!/bin/bash
# Script para ejecutar los tests del proyecto

set -e

# Colores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 Ejecutando tests del proyecto...${NC}\n"

# Ir al directorio raíz del proyecto
cd "$(dirname "$0")"

# Compilar si es necesario
if [ ! -f "build/tests/test_streaming_whisper" ]; then
    echo -e "${BLUE}📦 Compilando proyecto...${NC}"
    cmake -B build -DBUILD_TESTS=ON
    cmake --build build -j
fi

# Ejecutar tests
cd build/tests

echo -e "${YELLOW}▶ Tests de StreamingWhisperEngine${NC}"
./test_streaming_whisper "$@"

echo -e "\n${GREEN}✅ Todos los tests completados${NC}"
