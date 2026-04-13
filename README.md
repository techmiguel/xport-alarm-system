# XPort Alarm System — Sistema de Alarma 8 Zonas

[![Firmware](https://img.shields.io/badge/Firmware-C8051F320-blue?logo=c)](#)
[![PC App](https://img.shields.io/badge/App_PC-Python_3.8+-yellow?logo=python)](#)
[![Protocol](https://img.shields.io/badge/Protocolo-TCP%2FIP_via_XPort-green)](#)
[![License](https://img.shields.io/badge/Licencia-MIT-lightgrey)](#licencia)

Sistema embebido de seguridad con **8 zonas de alarma** controladas por un microcontrolador Silicon Labs **C8051F320**, con conectividad Ethernet a través de un módulo **Lantronix XPort**, y una aplicación de administración remota en **Python** con interfaz gráfica.

---

## Características principales

| Característica | Detalle |
|---|---|
| Zonas de alarma | 8 zonas independientes vía **P1** |
| Detección de falla de sensor | 8 líneas de supervisión vía **P2** (corte de cable / desconexión) |
| Conectividad | Ethernet TCP/IP mediante **Lantronix XPort** (UART→Ethernet) |
| Reset de IP por hardware | Botón en **P0.0** — mantener 3 s restaura IP por defecto |
| App PC | Python 3.8+, interfaz gráfica Tkinter, multiplataforma |
| Múltiples equipos | Identificación de dispositivos por nombre asignado a cada IP |
| Historial | Registro de eventos exportable a CSV |
| Seguridad | Autenticación de usuarios con roles (admin/operador) |

---

## Arquitectura del sistema

```
┌─────────────────────────────────────────────────────────────┐
│                     HARDWARE EMBEBIDO                       │
│                                                             │
│  P1[0..7] ──► Sensores de alarma (lazo NC)                 │
│  P2[0..7] ──► Detector de desconexión de sensor            │
│               (1 = conectado, 0 = falla/cable cortado)      │
│  P0.0     ──► Botón reset IP (activo LOW, hold 3s)         │
│  P0.4/P0.5 ─► UART0 ──► XPort ──► Ethernet TCP/IP         │
│                                                             │
│                [C8051F320 @ 12 MHz]                         │
└─────────────────────┬───────────────────────────────────────┘
                      │ TCP/IP  Puerto 10001
                      ▼
┌─────────────────────────────────────────────────────────────┐
│               APLICACIÓN PC (Python + Tkinter)              │
│                                                             │
│  • Monitor en tiempo real de 8 zonas                        │
│  • Estado: Normal / Alarma / Sensor desconectado            │
│  • Habilitar / deshabilitar zonas individualmente           │
│  • Gestión de nombres por IP de dispositivo                 │
│  • Historial de eventos con filtros y exportación CSV       │
│  • Autenticación multi-usuario (admin / operador)           │
│  • Reconexión automática                                     │
└─────────────────────────────────────────────────────────────┘
```

---

## Protocolo de comunicación UART / TCP

El firmware envía mensajes de texto plano terminados en `\r\n`. La aplicación PC los parsea en tiempo real.

### Mensajes espontáneos (firmware → PC)

| Mensaje | Significado |
|---|---|
| `Z<n>:<s>` | Cambio de estado zona `n` (0–7): `s=1` normal, `s=0` abierta |
| `!Z<n>` | Alarma activada en zona `n` |
| `C<n>:<s>` | Estado de conectividad zona `n`: `s=1` conectado, `s=0` desconectado |
| `IP_RST` | Botón de reset IP presionado 3 segundos |

### Comandos (PC → firmware)

| Comando | Respuesta | Descripción |
|---|---|---|
| `S` | `S <zonas> E:<hab> A:<alarma> K:<conex>` | Estado completo del sistema |
| `R` | `R OK` | Reset de alarma activa |
| `E <n>` | `E Z<n>` | Habilitar zona `n` |
| `D <n>` | `D Z<n>` | Deshabilitar zona `n` |
| `H` | `H S=Status R=Reset...` | Ayuda |

> Todos los valores en la respuesta de `S` son decimales.  
> `K` es la máscara de conectividad: bit `i` = 1 → sensor zona `i` conectado.

---

## Estructura del repositorio

```
xport-alarm-system/
├── firmware_80c51f320/
│   ├── main.c                      # Firmware principal (C, Keil C51)
│   ├── firmware_alarma.c           # Módulo auxiliar de firmware
│   ├── Alarma_XPort_8051F320.wsp   # Workspace Keil uVision
│   └── Alarma_XPort_8051F320.hex   # Binario compilado (flasheable)
│
├── SistemaAlarma8Zonas_PC/
│   ├── alarm_system.py             # Aplicación PC (fuente principal)
│   ├── requirements.txt            # Dependencias Python
│   ├── build_exe.bat               # Script para compilar a .exe (PyInstaller)
│   └── alarm_icon.ico              # Ícono de la aplicación
│
└── README.md
```

---

## Requisitos de hardware

- Microcontrolador **Silicon Labs C8051F320**
- Módulo Ethernet **Lantronix XPort** (configurado en puerto 10001, modo TCP server)
- 8 sensores de alarma (lazo normalmente cerrado) conectados a **P1.0 – P1.7**
- 8 circuitos detectores de desconexión conectados a **P2.0 – P2.7**
  - Salida `1` lógico → sensor conectado
  - Salida `0` lógico → sensor desconectado / cable cortado
- Pulsador de reset IP en **P0.0** (conectado a GND, con pull-up interno)

---

## Instalación — Aplicación PC

### Requisitos

- Python 3.8 o superior
- Conexión de red al dispositivo XPort

### Pasos

```bash
# Clonar el repositorio
git clone https://github.com/techmiguel/xport-alarm-system.git
cd xport-alarm-system/SistemaAlarma8Zonas_PC

# Instalar dependencias
pip install -r requirements.txt

# Ejecutar
python alarm_system.py
```

### Compilar a ejecutable Windows (.exe)

```bash
cd SistemaAlarma8Zonas_PC
build_exe.bat
# El ejecutable queda en dist/SistemaAlarma.exe
```

### Credenciales por defecto

Al ejecutar por primera vez se crea el usuario administrador:

| Usuario | Contraseña |
|---|---|
| `admin` | `admin123` |

> **Cambiar la contraseña después del primer inicio de sesión.**

---

## Instalación — Firmware

1. Abrir `firmware_80c51f320/Alarma_XPort_8051F320.wsp` con **Keil uVision** (soporte C8051F320)
2. Compilar el proyecto (`Build`)
3. Flashear `Alarma_XPort_8051F320.hex` al microcontrolador mediante el programador Silicon Labs

---

## Configuración del XPort

El módulo XPort debe configurarse con:

- **Modo**: TCP Server
- **Puerto local**: `10001`
- **Baud rate**: `9600`
- **Formato**: `8N1`
- **IP**: asignar IP fija dentro de la red local

El reset por hardware (botón P0.0) envía la señal `IP_RST` a la aplicación PC. La reconexión se debe hacer manualmente ingresando la IP por defecto configurada en el equipo.

---

## Licencia

Este proyecto está licenciado bajo la **MIT License**.
