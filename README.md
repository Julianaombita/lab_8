
# Práctica 8 – Interfaz OLED I2C y Telemetría Local
## Curso de Microprocesadores | STM32

---

## Descripción del sistema

Sistema embebido desarrollado sobre STM32 (Blue Pill / Nucleo) que integra:

- **Pantalla OLED 0.96" (SSD1306)** vía I2C para visualización local
- **Sensor MPU6050** (acelerómetro/giroscopio) en el mismo bus I2C
- **Sensor Sharp IR** (analógico, ADC) para detección de distancia
- **Sensor ultrasónico HC-SR04** para medición de distancia frontal
- **Sensor de impacto** (digital) para detección de colisión
- **UART** para telemetría serial / depuración
- **Botón** para cambio de modo mediante máquina de estados

### Modos de operación

| Modo | Descripción |
|------|-------------|
| **Técnico** | Muestra en pantalla y UART: distancia, ADC, impacto, aceleración |
| **Competencia** | Muestra expresiones gráficas (cara neutra / lucha / dolor) |

El cambio de modo se realiza con el botón (PC13) con debounce de 200 ms.

---

## Estructura del repositorio

```
practica9/
├── main.c              ← Código fuente principal
├── README.md           ← Este archivo
└── STM32F103_I2C.ioc   ← Configuración CubeMX (importable)
```

---

## Configuración de hardware (pines)

| Señal | Pin STM32 | Periférico |
|-------|-----------|------------|
| I2C1_SCL | PB6 | OLED + MPU6050 |
| I2C1_SDA | PB7 | OLED + MPU6050 |
| USART2_TX | PA2 | Terminal serial |
| USART2_RX | PA3 | Terminal serial |
| ADC1_CH0 | PA0 | Sharp IR |
| ECHO | PB0 | HC-SR04 |
| TRIG | PB1 | HC-SR04 |
| IMPACT | PA5 | Sensor impacto |
| BUTTON | PC13 | Cambio de modo |

---

## Direcciones I2C

| Dispositivo | Dirección (7-bit) |
|-------------|-------------------|
| OLED SSD1306 | 0x3C |
| MPU6050 | 0x68 |

---

## Compilación

1. Abrir **STM32CubeIDE**
2. Crear proyecto STM32F103C8 (Blue Pill)
3. Importar `STM32F103_I2C.ioc` o configurar manualmente (ver sección IOC)
4. Reemplazar el contenido de `Core/Src/main.c` con el archivo provisto
5. Compilar y flashear

---

## Configuración CubeMX (.ioc)

### I2C1
- Mode: **I2C**
- Speed: **100 kHz** (Standard Mode)
- SCL: PB6 | SDA: PB7

### USART2
- Mode: **Asynchronous**
- Baud Rate: **115200**
- Word Length: 8 bits | Stop: 1 | Parity: None

### ADC1
- Channel IN0 (PA0)
- Continuous: Disabled | Software trigger

### TIM2
- Prescaler: 71 → resolución de 1 µs (para HC-SR04)
- Period: 65535

### GPIO
| Pin | Label | Mode |
|-----|-------|------|
| PC13 | BTN | Input Pull-Up |
| PA5 | IMPACT | Input Pull-Up |
| PB1 | TRIG | Output PP |
| PB0 | ECHO | Input No Pull |

---

## Evidencia de funcionamiento

- 📹 Video Ejercicio 1–4: Inicialización I2C + OLED + texto
- 📹 Video Ejercicio 5–6: Lectura MPU6050 + telemetría pantalla
- 📹 Video Ejercicio 7–8: Modo competencia + cambio con botón

---
