# 🇺🇸 LogIQ Agent

# LogIQ Agent

> Lightweight, rotation-safe, ACK-driven log forwarder written in modern C++20.

LogIQ Agent is the open-source log collector of the **LogControlIQ** platform.
It is designed to reliably stream logs from local files to one or multiple backends with strong correctness guarantees.

Built from the ground up for production-grade environments.

---

## 🚀 Why LogIQ Agent?

Most log shippers focus on convenience.
LogIQ Agent focuses on **correctness and safety**.

* ✅ Inode-aware file tracking
* ✅ Safe log rotation handling (rename & copytruncate)
* ✅ Precise byte-range tracking per event
* ✅ ACK-driven checkpointing
* ✅ Modular, extensible architecture
* ✅ Modern C++20 implementation
 
No silent data loss.
No blind commits.
No unsafe rotation behavior.

---

## 🏗 Architecture Overview

LogIQ Agent follows a deterministic runtime pipeline:

```
OBSERVE → READ → FRAME → BATCH → SEND → ACK → COMMIT
```

Each event keeps exact file byte offsets:

```
[start_offset, end_offset)
```

Checkpoints advance **only after backend ACK**.

This ensures:

* No false commits
* No lost logs
* No duplication during restart

---

## 🔒 Rotation-Safe by Design

The FileFollower subsystem:

* Tracks files by `(device, inode)`
* Detects rename-based rotation
* Detects copytruncate / manual truncation
* Maintains file generation tracking
* Drains old files safely before switching

Designed for real-world production scenarios.

---

## 🧩 Modular Components

| Module     | Responsibility             |
| ---------- | -------------------------- |
| `core/`    | Agent orchestration        |
| `file/`    | Inode-aware file tracking  |
| `framing/` | Event framing (line-based) |
| `sinks/`   | Backend abstraction        |
| `router/`  | Multi-destination routing  |
| `config/`  | Configuration loader       |
| `utils/`   | Logging & utilities        |

---

## 🔌 Extensible Backend Support

LogIQ Agent uses a pluggable `Sink` abstraction.

Currently includes:

* HTTP NDJSON sink (minimal implementation)

Future targets:

* OTLP
* Kafka
* S3
* LogControlIQ native protocol

---

## ⚙️ Built With

* C++20
* Clang / GCC compatible
* CMake
* clangd-ready development workflow

No heavy runtime dependencies.

---

## 🛣 Roadmap

Next milestones:

* Persistent checkpoint storage
* Retry + backoff system
* Disk spool (WAL)
* Multi-sink routing
* AI-powered classification & enrichment
* Internal metrics & health endpoints

---

## 📦 Status

Early-stage but architecturally solid.
Designed for long-term scalability and correctness.

---

---

# 🇪🇸 LogIQ Agent

# LogIQ Agent

> Forwarder de logs ligero, seguro ante rotación y basado en confirmación ACK, desarrollado en C++20 moderno.

LogIQ Agent es el componente open source de la plataforma **LogControlIQ** encargado de recolectar y enviar logs de forma segura hacia uno o múltiples backends.

Diseñado desde cero para entornos de producción.

---

## 🚀 ¿Por qué LogIQ Agent?

La mayoría de forwarders priorizan simplicidad.
LogIQ Agent prioriza **correctitud y seguridad**.

* ✅ Seguimiento por inode (device + inode)
* ✅ Manejo seguro de rotación (rename y copytruncate)
* ✅ Rastreo preciso de offsets por evento
* ✅ Checkpoint basado en ACK real
* ✅ Arquitectura modular y extensible
* ✅ Implementado en C++20 moderno

Sin pérdida silenciosa de datos.
Sin commits prematuros.
Sin comportamiento inseguro ante rotación.

---

## 🏗 Arquitectura

El pipeline interno del agente sigue el patrón:

```
OBSERVE → READ → FRAME → BATCH → SEND → ACK → COMMIT
```

Cada evento conserva su rango exacto de bytes:

```
[start_offset, end_offset)
```

El checkpoint avanza **solo cuando el backend confirma recepción**.

---

## 🔒 Seguro ante Rotación

El subsistema FileFollower:

* Identifica archivos por `(device, inode)`
* Detecta rotación por rename
* Detecta copytruncate
* Maneja generación de archivo
* Drena archivos antiguos antes de cambiar

Preparado para escenarios reales de producción.

---

## 🧩 Componentes

| Módulo     | Función                        |
| ---------- | ------------------------------ |
| `core/`    | Orquestación del agente        |
| `file/`    | Seguimiento seguro de archivos |
| `framing/` | Separación de eventos          |
| `sinks/`   | Envío a backends               |
| `router/`  | Enrutamiento multi-destino     |
| `config/`  | Carga de configuración         |
| `utils/`   | Logging y utilidades           |

---

## 🛣 Próximos Pasos

* Persistencia de checkpoint en disco
* Sistema de reintentos
* Spool local (WAL)
* Multi-sink
* Clasificación con IA
* Métricas internas

