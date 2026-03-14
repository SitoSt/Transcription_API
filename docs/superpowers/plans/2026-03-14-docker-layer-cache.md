# Docker Layer Cache Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reestructurar el Dockerfile para que whisper.cpp/CUDA solo se recompile cuando cambia el submodule, y añadir un job de CI que cachea capas Docker en GitHub Actions.

**Architecture:** Dividir el builder stage en dos fases con `COPY` separados: Fase A copia `third_party/` y `CMakeLists.txt`, configura cmake y compila `--target whisper`; Fase B copia `src/` y compila `--target jota-transcriber`. Docker layer cache mantiene Fase A intacta cuando solo cambia código fuente.

**Tech Stack:** Docker multi-stage builds, CMake, GitHub Actions (`docker/setup-buildx-action`, `docker/build-push-action`)

---

## Chunk 1: Reestructurar Dockerfile

### Task 1: Dividir el builder stage en Fase A y Fase B

**Files:**
- Modify: `Dockerfile`

El Dockerfile actual tiene un único `COPY . .` que invalida todo. Hay que reemplazar el builder stage entero.

- [ ] **Step 1: Leer el Dockerfile actual**

```bash
cat Dockerfile
```

Confirmar que el builder stage empieza en la línea 1 y el runtime stage en la línea 31.

- [ ] **Step 2: Reemplazar el builder stage**

Reemplazar **todo el builder stage** (desde `# Build Stage` hasta la línea justo antes de `# Runtime Stage`, incluyendo el `FROM`, el `apt-get`, el `WORKDIR`, el `COPY . .` y el `RUN cmake`) con:

```dockerfile
# Build Stage
FROM nvidia/cuda:12.3.1-devel-ubuntu22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Instalamos dependencias de compilación
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Stubs de CUDA driver necesarios para linkar libggml-cuda
ENV LIBRARY_PATH="/usr/local/cuda/lib64/stubs:${LIBRARY_PATH}"

# ── Fase A: whisper.cpp + dependencias externas ────────────────────────────
# Esta capa solo se invalida cuando cambia third_party/ o CMakeLists.txt.
# Configura con BUILD_SERVER=ON para que FetchContent descargue nlohmann/json
# aquí (cmake no verifica existencia de .cpp al configurar, solo al compilar).
# Boost y OpenSSL ya están instalados arriba — los find_package pasan sin error.
COPY third_party/ third_party/
COPY CMakeLists.txt .
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SERVER=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DGGML_CUDA=1

# Compila whisper + ggml (incluye kernels CUDA — la parte lenta).
RUN cmake --build build --target whisper -j$(nproc)

# ── Fase B: código del servidor ────────────────────────────────────────────
# Solo se invalida cuando cambia src/ o generate_certs.sh.
# cmake detecta que whisper/ggml ya están compilados y los omite.
COPY src/ src/
COPY generate_certs.sh .
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SERVER=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DGGML_CUDA=1
RUN cmake --build build --target jota-transcriber -j$(nproc)
```

El runtime stage (desde `# Runtime Stage` en adelante) no se toca.

- [ ] **Step 3: Verificar la estructura del Dockerfile resultante**

```bash
grep -n "FROM\|COPY\|RUN cmake\|Fase" Dockerfile
```

Salida esperada (líneas aproximadas):
```
1:FROM nvidia/cuda:12.3.1-devel-ubuntu22.04 AS builder
...
(Fase A) COPY third_party/
(Fase A) COPY CMakeLists.txt
(Fase A) RUN cmake -B build ...
(Fase A) RUN cmake --build build --target whisper
(Fase B) COPY src/
(Fase B) COPY generate_certs.sh
(Fase B) RUN cmake -B build ...
(Fase B) RUN cmake --build build --target jota-transcriber
FROM nvidia/cuda:12.3.1-runtime-ubuntu22.04 AS runtime
COPY --from=builder ...
COPY --from=builder ...
```

- [ ] **Step 4: Verificar que el runtime stage está intacto**

```bash
grep -A 20 "Runtime Stage" Dockerfile
```

Debe mostrar las mismas líneas que antes: `apt-get install libboost-system...`, `COPY --from=builder`, `useradd`, `EXPOSE 8003`, `CMD`.

- [ ] **Step 5: Commit**

```bash
git add Dockerfile
git commit -m "perf: restructure Dockerfile builder into two COPY phases for layer caching"
```

---

## Chunk 2: Job de Docker build en CI

### Task 2: Añadir job docker-build a ci.yml

**Files:**
- Modify: `.github/workflows/ci.yml`

Añadir un segundo job al workflow que construye la imagen Docker usando BuildKit con caché de GHA. El job existente `build-and-test` no se toca.

- [ ] **Step 1: Añadir el job docker-build al workflow**

Al final del fichero `.github/workflows/ci.yml` (después del job `build-and-test`), añadir:

```yaml
  docker-build:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Build Docker image (with layer cache)
        uses: docker/build-push-action@v6
        with:
          context: .
          push: false
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

La clave `docker-build:` debe estar al mismo nivel de indentación que `build-and-test:` (sin espacios antes, directamente bajo `jobs:`).

- [ ] **Step 2: Verificar YAML válido**

```bash
python3 -c "import yaml, sys; yaml.safe_load(open('.github/workflows/ci.yml')); print('YAML OK')"
```

Salida esperada: `YAML OK`

- [ ] **Step 3: Verificar que el job tiene la estructura correcta**

```bash
grep -n "jobs:\|build-and-test:\|docker-build:\|setup-buildx\|build-push-action\|cache-from\|cache-to" .github/workflows/ci.yml
```

Debe mostrar ambos jobs y las claves de caché del nuevo job.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add docker-build job with GHA layer cache"
```

---

## Chunk 3: Verificación

### Task 3: Verificar que el build funciona localmente (sin GPU)

**Nota:** Si no hay GPU disponible en el entorno de verificación, el build con `-DGGML_CUDA=1` fallará en los kernels CUDA. En ese caso, verificar la estructura del Dockerfile y la sintaxis de ci.yml es suficiente — el build real se valida en el entorno con GPU del usuario.

- [ ] **Step 1: Verificar que no quedan referencias al COPY . . problemático**

```bash
grep -n "COPY \. \." Dockerfile
```

Salida esperada: sin output (no debe haber `COPY . .`).

- [ ] **Step 2: Comprobar que los dos COPY de Fase A y Fase B están en orden correcto**

```bash
grep -n "^COPY" Dockerfile
```

Salida esperada (en este orden):
```
XX:COPY third_party/ third_party/
XX:COPY CMakeLists.txt .
XX:COPY src/ src/
XX:COPY generate_certs.sh .
```

Y en el runtime stage:
```
XX:COPY --from=builder /app/build/jota-transcriber /app/jota-transcriber
XX:COPY --from=builder /app/generate_certs.sh /app/generate_certs.sh
```

- [ ] **Step 3: Verificar ci.yml tiene los dos jobs**

```bash
grep "^\s*[a-z-]*:$" .github/workflows/ci.yml | grep -v "steps:\|with:\|on:"
```

Debe mostrar `build-and-test:` y `docker-build:`.

- [ ] **Step 4: Intentar docker build sin GPU (opcional, si hay Docker disponible)**

Prerequisito: asegurarse de que los submodules están inicializados:
```bash
git submodule update --init --recursive
```

Luego lanzar el build:
```bash
DOCKER_BUILDKIT=1 docker build --no-cache \
  -t jota-transcriber-test \
  --target builder \
  . 2>&1 | tail -20
```

Si no hay GPU, fallará en el paso `cmake --build ... --target whisper` con error de CUDA — eso es esperado. Lo importante es que Fase A (configure + FetchContent descarga nlohmann/json) pase sin errores antes del paso de compilación.

- [ ] **Step 5: Commit del plan (si no está ya commiteado)**

```bash
git add docs/superpowers/plans/2026-03-14-docker-layer-cache.md
git commit -m "docs: add Docker layer cache implementation plan"
```
