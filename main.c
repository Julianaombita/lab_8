/* ===========================================================
 * Práctica 8: Interfaz OLED I2C y Telemetría Local
 * Curso de Microprocesadores - STM32
 * ===========================================================
 * Descripción:
 *   Sistema integrado con pantalla OLED SSD1306 vía I2C,
 *   sensor I2C adicional (MPU6050), sensores Sharp IR,
 *   ultrasónico, ADC y sensor de impacto. Soporta modo
 *   técnico (telemetría) y modo competencia (expresiones).
 * =========================================================== */

#include "main.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

/* ─── Handles de periféricos (generados por CubeMX) ───────── */
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart2;
ADC_HandleTypeDef  hadc1;
TIM_HandleTypeDef  htim2;   /* Timer para trigger ultrasónico  */

/* ─── Definiciones OLED SSD1306 ──────────────────────────── */
#define OLED_ADDR         (0x3C << 1)
#define OLED_WIDTH         128
#define OLED_HEIGHT         64
#define OLED_PAGES           8

/* ─── Definiciones MPU6050 ───────────────────────────────── */
#define MPU6050_ADDR      (0x68 << 1)
#define MPU6050_PWR_MGMT   0x6B
#define MPU6050_ACCEL_XOUT 0x3B

/* ─── Pines GPIO (ajustar según CubeMX) ──────────────────── */
#define BTN_PIN           GPIO_PIN_13
#define BTN_PORT          GPIOC
#define TRIG_PIN          GPIO_PIN_1
#define TRIG_PORT         GPIOB
#define ECHO_PIN          GPIO_PIN_0
#define ECHO_PORT         GPIOB
#define IMPACT_PIN        GPIO_PIN_5
#define IMPACT_PORT       GPIOA

/* ─── Máquina de estados ─────────────────────────────────── */
typedef enum { MODE_TECNICO = 0, MODE_COMPETENCIA } SystemMode;

/* ─── Expresiones modo competencia ──────────────────────── */
typedef enum { FACE_NEUTRAL = 0, FACE_FIGHT, FACE_PAIN } FaceType;

/* ─── Variables globales ─────────────────────────────────── */
static uint8_t    oled_buffer[1024];
static SystemMode currentMode     = MODE_TECNICO;
static FaceType   currentFace     = FACE_NEUTRAL;
static uint8_t    btnPressed      = 0;
static uint32_t   lastDebounce    = 0;

/* ─── Fuente 5x7 básica (ASCII 32–127) ──────────────────── */
/* Sólo se incluyen algunos caracteres clave; completar según necesidad */
static const uint8_t font5x7[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, /* ' ' 32 */
  {0x00,0x00,0x5F,0x00,0x00}, /* '!' 33 */
  {0x00,0x07,0x00,0x07,0x00}, /* '"' 34 */
  {0x14,0x7F,0x14,0x7F,0x14}, /* '#' 35 */
  {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' 36 */
  {0x23,0x13,0x08,0x64,0x62}, /* '%' 37 */
  {0x36,0x49,0x55,0x22,0x50}, /* '&' 38 */
  {0x00,0x05,0x03,0x00,0x00}, /* ''' 39 */
  {0x00,0x1C,0x22,0x41,0x00}, /* '(' 40 */
  {0x00,0x41,0x22,0x1C,0x00}, /* ')' 41 */
  {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' 42 */
  {0x08,0x08,0x3E,0x08,0x08}, /* '+' 43 */
  {0x00,0x50,0x30,0x00,0x00}, /* ',' 44 */
  {0x08,0x08,0x08,0x08,0x08}, /* '-' 45 */
  {0x00,0x60,0x60,0x00,0x00}, /* '.' 46 */
  {0x20,0x10,0x08,0x04,0x02}, /* '/' 47 */
  {0x3E,0x51,0x49,0x45,0x3E}, /* '0' 48 */
  {0x00,0x42,0x7F,0x40,0x00}, /* '1' 49 */
  {0x42,0x61,0x51,0x49,0x46}, /* '2' 50 */
  {0x21,0x41,0x45,0x4B,0x31}, /* '3' 51 */
  {0x18,0x14,0x12,0x7F,0x10}, /* '4' 52 */
  {0x27,0x45,0x45,0x45,0x39}, /* '5' 53 */
  {0x3C,0x4A,0x49,0x49,0x30}, /* '6' 54 */
  {0x01,0x71,0x09,0x05,0x03}, /* '7' 55 */
  {0x36,0x49,0x49,0x49,0x36}, /* '8' 56 */
  {0x06,0x49,0x49,0x29,0x1E}, /* '9' 57 */
  {0x00,0x36,0x36,0x00,0x00}, /* ':' 58 */
  {0x00,0x56,0x36,0x00,0x00}, /* ';' 59 */
  {0x00,0x08,0x14,0x22,0x41}, /* '<' 60 */
  {0x14,0x14,0x14,0x14,0x14}, /* '=' 61 */
  {0x41,0x22,0x14,0x08,0x00}, /* '>' 62 */
  {0x02,0x01,0x51,0x09,0x06}, /* '?' 63 */
  {0x32,0x49,0x79,0x41,0x3E}, /* '@' 64 */
  {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' 65 */
  {0x7F,0x49,0x49,0x49,0x36}, /* 'B' 66 */
  {0x3E,0x41,0x41,0x41,0x22}, /* 'C' 67 */
  {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' 68 */
  {0x7F,0x49,0x49,0x49,0x41}, /* 'E' 69 */
  {0x7F,0x09,0x09,0x09,0x01}, /* 'F' 70 */
  {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' 71 */
  {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' 72 */
  {0x00,0x41,0x7F,0x41,0x00}, /* 'I' 73 */
  {0x20,0x40,0x41,0x3F,0x01}, /* 'J' 74 */
  {0x7F,0x08,0x14,0x22,0x41}, /* 'K' 75 */
  {0x7F,0x40,0x40,0x40,0x40}, /* 'L' 76 */
  {0x7F,0x02,0x04,0x02,0x7F}, /* 'M' 77 */
  {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' 78 */
  {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' 79 */
  {0x7F,0x09,0x09,0x09,0x06}, /* 'P' 80 */
  {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' 81 */
  {0x7F,0x09,0x19,0x29,0x46}, /* 'R' 82 */
  {0x46,0x49,0x49,0x49,0x31}, /* 'S' 83 */
  {0x01,0x01,0x7F,0x01,0x01}, /* 'T' 84 */
  {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' 85 */
  {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' 86 */
  {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' 87 */
  {0x63,0x14,0x08,0x14,0x63}, /* 'X' 88 */
  {0x07,0x08,0x70,0x08,0x07}, /* 'Y' 89 */
  {0x61,0x51,0x49,0x45,0x43}, /* 'Z' 90 */
  {0x00,0x7F,0x41,0x41,0x00}, /* '[' 91 */
  {0x02,0x04,0x08,0x10,0x20}, /* '\' 92 */
  {0x00,0x41,0x41,0x7F,0x00}, /* ']' 93 */
  {0x04,0x02,0x01,0x02,0x04}, /* '^' 94 */
  {0x40,0x40,0x40,0x40,0x40}, /* '_' 95 */
  {0x00,0x01,0x02,0x04,0x00}, /* '`' 96 */
  {0x20,0x54,0x54,0x54,0x78}, /* 'a' 97 */
  {0x7F,0x48,0x44,0x44,0x38}, /* 'b' 98 */
  {0x38,0x44,0x44,0x44,0x20}, /* 'c' 99 */
  {0x38,0x44,0x44,0x48,0x7F}, /* 'd' 100 */
  {0x38,0x54,0x54,0x54,0x18}, /* 'e' 101 */
  {0x08,0x7E,0x09,0x01,0x02}, /* 'f' 102 */
  {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' 103 */
  {0x7F,0x08,0x04,0x04,0x78}, /* 'h' 104 */
  {0x00,0x44,0x7D,0x40,0x00}, /* 'i' 105 */
  {0x20,0x40,0x44,0x3D,0x00}, /* 'j' 106 */
  {0x7F,0x10,0x28,0x44,0x00}, /* 'k' 107 */
  {0x00,0x41,0x7F,0x40,0x00}, /* 'l' 108 */
  {0x7C,0x04,0x18,0x04,0x78}, /* 'm' 109 */
  {0x7C,0x08,0x04,0x04,0x78}, /* 'n' 110 */
  {0x38,0x44,0x44,0x44,0x38}, /* 'o' 111 */
  {0x7C,0x14,0x14,0x14,0x08}, /* 'p' 112 */
  {0x08,0x14,0x14,0x18,0x7C}, /* 'q' 113 */
  {0x7C,0x08,0x04,0x04,0x08}, /* 'r' 114 */
  {0x48,0x54,0x54,0x54,0x20}, /* 's' 115 */
  {0x04,0x3F,0x44,0x40,0x20}, /* 't' 116 */
  {0x3C,0x40,0x40,0x40,0x7C}, /* 'u' 117 */
  {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' 118 */
  {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' 119 */
  {0x44,0x28,0x10,0x28,0x44}, /* 'x' 120 */
  {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' 121 */
  {0x44,0x64,0x54,0x4C,0x44}, /* 'z' 122 */
  {0x00,0x08,0x36,0x41,0x00}, /* '{' 123 */
  {0x00,0x00,0x7F,0x00,0x00}, /* '|' 124 */
  {0x00,0x41,0x36,0x08,0x00}, /* '}' 125 */
  {0x08,0x08,0x2A,0x1C,0x08}, /* '~' 126 */
};

/* ═══════════════════════════════════════════════════════════
 *  PROTOTIPOS
 * ═══════════════════════════════════════════════════════════ */
/* I2C utils */
void     i2c_scan(void);

/* OLED */
void     ssd1306_init(void);
void     ssd1306_clear(void);
void     ssd1306_update(void);
void     ssd1306_draw_pixel(int x, int y);
void     ssd1306_draw_char(int x, int y, char c);
void     ssd1306_draw_string(int x, int y, const char *str);
void     ssd1306_draw_hline(int x, int y, int len);
void     ssd1306_draw_rect(int x, int y, int w, int h);
void     ssd1306_fill_circle(int cx, int cy, int r);
void     ssd1306_draw_bitmap(int x, int y,
                              const uint8_t *bmp, int w, int h);

/* Sensor MPU6050 */
void     mpu6050_init(void);
void     mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az);

/* Sensores adicionales */
uint32_t hcsr04_read_cm(void);
uint16_t sharp_read_adc(void);
uint8_t  impact_read(void);

/* Modos */
void     mode_tecnico_update(void);
void     mode_competencia_update(void);

/* Caras */
void     oled_draw_face_neutral(void);
void     oled_draw_face_fight(void);
void     oled_draw_face_pain(void);

/* UART helper */
void     uart_print(const char *msg);

/* HAL callbacks y privadas */
void     SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();

  HAL_TIM_Base_Start(&htim2);

  /* ── Ejercicio 1: I2C listo ── */
  uart_print("I2C Ready\r\n");

  /* ── Ejercicio 2: Escaneo del bus ── */
  i2c_scan();

  /* ── Ejercicio 3: Inicializar OLED ── */
  ssd1306_init();
  uart_print("OLED Ready\r\n");

  /* ── Ejercicio 4: Primer texto en pantalla ── */
  ssd1306_clear();
  ssd1306_draw_string(0, 0, "Modo Tecnico");
  ssd1306_update();

  /* ── Ejercicio 5: Inicializar MPU6050 ── */
  mpu6050_init();
  uart_print("MPU6050 Ready\r\n");

  /* ── Loop principal ── */
  while (1)
  {
    /* Detección de botón con debounce (Ejercicio 8) */
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_RESET)
    {
      if ((HAL_GetTick() - lastDebounce) > 200)
      {
        lastDebounce = HAL_GetTick();
        currentMode  = (currentMode == MODE_TECNICO)
                       ? MODE_COMPETENCIA : MODE_TECNICO;
        ssd1306_clear();
      }
    }

    if (currentMode == MODE_TECNICO)
      mode_tecnico_update();
    else
      mode_competencia_update();

    HAL_Delay(200);
  }
}

/* ═══════════════════════════════════════════════════════════
 *  EJERCICIO 2: Escaneo del bus I2C
 * ═══════════════════════════════════════════════════════════ */
void i2c_scan(void)
{
  char buf[48];
  uart_print("Scanning I2C bus...\r\n");

  for (uint8_t addr = 1; addr < 128; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK)
    {
      snprintf(buf, sizeof(buf),
               "I2C Device found at 0x%02X\r\n", addr);
      uart_print(buf);
    }
  }
  uart_print("Scan complete.\r\n");
}

/* ═══════════════════════════════════════════════════════════
 *  OLED – Inicialización (Ejercicio 3)
 * ═══════════════════════════════════════════════════════════ */
void ssd1306_init(void)
{
  HAL_Delay(100);
  uint8_t cmds[] = {
    0x00,
    0xAE,        /* Display OFF              */
    0xD5, 0x80,  /* Clock divide ratio       */
    0xA8, 0x3F,  /* Multiplex ratio 64       */
    0xD3, 0x00,  /* Display offset = 0       */
    0x40,        /* Start line = 0           */
    0xA1,        /* Segment re-map           */
    0xC8,        /* COM scan dir remapped    */
    0xDA, 0x12,  /* COM pins hardware config */
    0x81, 0x7F,  /* Contrast                 */
    0xA4,        /* Display follows RAM      */
    0xA6,        /* Normal (non-inverted)    */
    0x20, 0x00,  /* Horizontal addressing    */
    0x8D, 0x14,  /* Charge pump enable       */
    0xAF         /* Display ON               */
  };
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR,
                           cmds, sizeof(cmds), HAL_MAX_DELAY);
}

/* ─── Limpiar buffer ─────────────────────────────────────── */
void ssd1306_clear(void)
{
  memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

/* ─── Enviar buffer a la pantalla ───────────────────────── */
void ssd1306_update(void)
{
  for (uint8_t page = 0; page < OLED_PAGES; page++)
  {
    uint8_t cmd[4] = {
      0x00,
      (uint8_t)(0xB0 + page), /* Página */
      0x00,                    /* Lower column start */
      0x10                     /* Higher column start */
    };
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR,
                             cmd, sizeof(cmd), HAL_MAX_DELAY);

    uint8_t data[129];
    data[0] = 0x40; /* Control byte: datos */
    memcpy(&data[1], &oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR,
                             data, sizeof(data), HAL_MAX_DELAY);
  }
}

/* ─── Dibujar píxel ─────────────────────────────────────── */
void ssd1306_draw_pixel(int x, int y)
{
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    return;
  oled_buffer[(y / 8) * OLED_WIDTH + x] |= (1 << (y % 8));
}

/* ─── Dibujar carácter (Ejercicio 4) ─────────────────────── */
void ssd1306_draw_char(int x, int y, char c)
{
  if (c < 32 || c > 122) return;
  int idx = c - 32;
  for (int i = 0; i < 5; i++)
    oled_buffer[x + i + (y / 8) * OLED_WIDTH] = font5x7[idx][i];
}

/* ─── Dibujar cadena ─────────────────────────────────────── */
void ssd1306_draw_string(int x, int y, const char *str)
{
  while (*str)
  {
    if (x + 6 > OLED_WIDTH) { x = 0; y += 8; }
    ssd1306_draw_char(x, y, *str++);
    x += 6;
  }
}

/* ─── Línea horizontal ───────────────────────────────────── */
void ssd1306_draw_hline(int x, int y, int len)
{
  for (int i = 0; i < len; i++) ssd1306_draw_pixel(x + i, y);
}

/* ─── Rectángulo ─────────────────────────────────────────── */
void ssd1306_draw_rect(int x, int y, int w, int h)
{
  for (int i = x; i < x + w; i++) {
    ssd1306_draw_pixel(i, y);
    ssd1306_draw_pixel(i, y + h - 1);
  }
  for (int j = y; j < y + h; j++) {
    ssd1306_draw_pixel(x, j);
    ssd1306_draw_pixel(x + w - 1, j);
  }
}

/* ─── Círculo relleno (usado para ojos) ──────────────────── */
void ssd1306_fill_circle(int cx, int cy, int r)
{
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x*x + y*y <= r*r)
        ssd1306_draw_pixel(cx + x, cy + y);
}

/* ─── Bitmap ─────────────────────────────────────────────── */
void ssd1306_draw_bitmap(int x, int y,
                          const uint8_t *bmp, int w, int h)
{
  for (int j = 0; j < h / 8; j++)
    for (int i = 0; i < w; i++)
      oled_buffer[(y / 8 + j) * OLED_WIDTH + (x + i)]
        = bmp[j * w + i];
}

/* ═══════════════════════════════════════════════════════════
 *  EJERCICIO 5: MPU6050
 * ═══════════════════════════════════════════════════════════ */
void mpu6050_init(void)
{
  uint8_t data[2] = {MPU6050_PWR_MGMT, 0x00}; /* Wake up */
  HAL_I2C_Master_Transmit(&hi2c1, MPU6050_ADDR,
                           data, 2, HAL_MAX_DELAY);
}

void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
  uint8_t reg = MPU6050_ACCEL_XOUT;
  uint8_t buf[6];
  HAL_I2C_Master_Transmit(&hi2c1, MPU6050_ADDR,
                           &reg, 1, HAL_MAX_DELAY);
  HAL_I2C_Master_Receive(&hi2c1, MPU6050_ADDR,
                          buf, 6, HAL_MAX_DELAY);
  *ax = (int16_t)(buf[0] << 8 | buf[1]);
  *ay = (int16_t)(buf[2] << 8 | buf[3]);
  *az = (int16_t)(buf[4] << 8 | buf[5]);
}

/* ═══════════════════════════════════════════════════════════
 *  SENSORES ADICIONALES
 * ═══════════════════════════════════════════════════════════ */

/* HC-SR04: Distancia en cm */
uint32_t hcsr04_read_cm(void)
{
  /* Pulso TRIGGER 10 µs */
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  while (__HAL_TIM_GET_COUNTER(&htim2) < 10);
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

  /* Esperar ECHO */
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET)
    if (__HAL_TIM_GET_COUNTER(&htim2) > 30000) return 999;

  __HAL_TIM_SET_COUNTER(&htim2, 0);
  while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
    if (__HAL_TIM_GET_COUNTER(&htim2) > 30000) return 999;

  return __HAL_TIM_GET_COUNTER(&htim2) / 58;
}

/* Sharp IR: Lectura ADC (PA0) */
uint16_t sharp_read_adc(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10);
  return HAL_ADC_GetValue(&hadc1);
}

/* Sensor de impacto (digital) */
uint8_t impact_read(void)
{
  return (HAL_GPIO_ReadPin(IMPACT_PORT, IMPACT_PIN)
          == GPIO_PIN_RESET) ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════
 *  EJERCICIO 6: Modo Técnico – Telemetría local
 * ═══════════════════════════════════════════════════════════ */
void mode_tecnico_update(void)
{
  char buf[32];
  int16_t ax, ay, az;

  uint32_t dist   = hcsr04_read_cm();
  uint16_t sharp  = sharp_read_adc();
  uint8_t  impact = impact_read();
  mpu6050_read_accel(&ax, &ay, &az);

  /* UART telemetría */
  snprintf(buf, sizeof(buf),
           "[T] D:%lucm S:%u Imp:%u\r\n", dist, sharp, impact);
  uart_print(buf);
  snprintf(buf, sizeof(buf),
           "[T] AX:%d AY:%d AZ:%d\r\n", ax, ay, az);
  uart_print(buf);

  /* Pantalla OLED */
  ssd1306_clear();
  ssd1306_draw_string(0, 0,  "== MODO TECNICO ==");
  ssd1306_draw_hline(0, 9, 128);

  snprintf(buf, sizeof(buf), "Dist: %lu cm", dist);
  ssd1306_draw_string(0, 16, buf);

  snprintf(buf, sizeof(buf), "ADC : %u", sharp);
  ssd1306_draw_string(0, 24, buf);

  snprintf(buf, sizeof(buf), "Imp : %s", impact ? "SI" : "NO");
  ssd1306_draw_string(0, 32, buf);

  snprintf(buf, sizeof(buf), "AX:%d", ax / 100);
  ssd1306_draw_string(0, 40, buf);
  snprintf(buf, sizeof(buf), "AY:%d", ay / 100);
  ssd1306_draw_string(44, 40, buf);

  ssd1306_update();

  /* Cambiar cara si hay impacto */
  if (currentMode == MODE_COMPETENCIA)
    currentFace = impact ? FACE_PAIN : FACE_FIGHT;
}

/* ═══════════════════════════════════════════════════════════
 *  EJERCICIO 7: Caras gráficas (SumoBot)
 * ═══════════════════════════════════════════════════════════ */

/* Cara neutra */
void oled_draw_face_neutral(void)
{
  /* Cabeza */
  ssd1306_draw_rect(24, 4, 80, 56);
  /* Ojos */
  ssd1306_fill_circle(44, 24, 6);
  ssd1306_fill_circle(84, 24, 6);
  /* Boca recta */
  ssd1306_draw_hline(44, 44, 40);
}

/* Cara de lucha */
void oled_draw_face_fight(void)
{
  /* Cabeza */
  ssd1306_draw_rect(24, 4, 80, 56);
  /* Ojos con cejas anguladas (agresivo) */
  ssd1306_fill_circle(44, 26, 6);
  ssd1306_fill_circle(84, 26, 6);
  /* Cejas */
  for (int i = 0; i < 12; i++) {
    ssd1306_draw_pixel(34 + i, 16 - i / 4);
    ssd1306_draw_pixel(78 + i, 12 + i / 4);
  }
  /* Boca de lucha (dientes) */
  ssd1306_draw_hline(40, 44, 48);
  for (int t = 0; t < 6; t++)
    ssd1306_draw_pixel(40 + t * 8, 50);
}

/* Cara de dolor */
void oled_draw_face_pain(void)
{
  /* Cabeza */
  ssd1306_draw_rect(24, 4, 80, 56);
  /* Ojos X */
  for (int i = -5; i <= 5; i++) {
    ssd1306_draw_pixel(44 + i, 24 + i);
    ssd1306_draw_pixel(44 + i, 24 - i);
    ssd1306_draw_pixel(84 + i, 24 + i);
    ssd1306_draw_pixel(84 + i, 24 - i);
  }
  /* Boca curvada hacia abajo */
  for (int i = -20; i <= 20; i++)
    ssd1306_draw_pixel(64 + i, 46 + (i * i) / 80);
  /* Estrellas de impacto */
  for (int d = 0; d < 4; d++) {
    int px = 14 + d * 6;
    ssd1306_draw_pixel(px,     10);
    ssd1306_draw_pixel(px + 2, 8);
    ssd1306_draw_pixel(px - 2, 8);
  }
}

/* ═══════════════════════════════════════════════════════════
 *  EJERCICIO 7+8: Modo Competencia
 * ═══════════════════════════════════════════════════════════ */
void mode_competencia_update(void)
{
  uint8_t impact = impact_read();
  uint32_t dist  = hcsr04_read_cm();

  /* Lógica de expresiones */
  if (impact)
    currentFace = FACE_PAIN;
  else if (dist < 20)
    currentFace = FACE_FIGHT;
  else
    currentFace = FACE_NEUTRAL;

  ssd1306_clear();

  switch (currentFace) {
    case FACE_NEUTRAL: oled_draw_face_neutral(); break;
    case FACE_FIGHT:   oled_draw_face_fight();   break;
    case FACE_PAIN:    oled_draw_face_pain();     break;
  }

  ssd1306_update();

  /* UART debug */
  char buf[32];
  snprintf(buf, sizeof(buf),
           "[C] Cara:%d Dist:%lu Imp:%u\r\n",
           currentFace, dist, impact);
  uart_print(buf);
}

/* ═══════════════════════════════════════════════════════════
 *  UART helper
 * ═══════════════════════════════════════════════════════════ */
void uart_print(const char *msg)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)msg,
                    strlen(msg), HAL_MAX_DELAY);
}

/* ═══════════════════════════════════════════════════════════
 *  CONFIGURACIÓN DE PERIFÉRICOS (MX)
 * ═══════════════════════════════════════════════════════════ */

/* I2C1 – 100 kHz, modo maestro */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 100000;          /* 100 kHz  */
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    Error_Handler();
}

/* USART2 – 115200 bps */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 115200;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
    Error_Handler();
}

/* ADC1 – Canal 0 (PA0) para Sharp IR */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance                   = ADC1;
  hadc1.Init.ScanConvMode          = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
    Error_Handler();

  sConfig.Channel      = ADC_CHANNEL_0;
  sConfig.Rank         = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    Error_Handler();
}

/* TIM2 – Contador de µs para HC-SR04 */
static void MX_TIM2_Init(void)
{
  htim2.Instance               = TIM2;
  htim2.Init.Prescaler         = 71;   /* 72 MHz / 72 = 1 MHz → 1 µs */
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 0xFFFF;
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    Error_Handler();
}

/* GPIO */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Botón PC13 (pull-up interno, activo en LOW) */
  GPIO_InitStruct.Pin  = BTN_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_PORT, &GPIO_InitStruct);

  /* Sensor impacto PA5 */
  GPIO_InitStruct.Pin  = IMPACT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(IMPACT_PORT, &GPIO_InitStruct);

  /* TRIG HC-SR04 PB1 */
  GPIO_InitStruct.Pin   = TRIG_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TRIG_PORT, &GPIO_InitStruct);

  /* ECHO HC-SR04 PB0 */
  GPIO_InitStruct.Pin  = ECHO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ECHO_PORT, &GPIO_InitStruct);
}

/* ─── Error Handler ──────────────────────────────────────── */
void Error_Handler(void)
{
  __disable_irq();
  while (1);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
