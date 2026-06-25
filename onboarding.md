# Visor Console — Onboarding

## Descripcion general

**Visor Console** es una aplicacion de diagnostico y monitoreo en tiempo real para barras de sensores de seguimiento de linea (sensores de vision robotica). Se comunica con hardware Arduino a traves de una conexion serial (COM) y muestra lecturas de sensores, datos de calibracion y calculos de posicion.

---

## Estructura del proyecto

```
C:\Cerea_K\visor_console\
├── src/
│   ├── main.c           Punto de entrada — loop principal del menu
│   ├── ui.c / ui.h      Interfaz de usuario y renderizado de menus
│   ├── serial.c / serial.h   Comunicacion serial con Windows API
│   ├── config.c / config.h   Gestion del archivo de configuracion
│   ├── sensor_bar.c / sensor_bar.h   Parseo y procesamiento de datos del sensor
│   ├── diagnostic.c / diagnostic.h  Clasificacion de estados del sensor
│   └── utils.c / utils.h    Funciones utilitarias (temporizado)
├── arduino/
│   ├── qtr8rc_visor/    Sketch Arduino para barra QTR-8RC (8 sensores)
│   └── im_hem_005/      Sketch Arduino para barra IM-HEM-005 (16 sensores)
├── Makefile
└── visor.exe
```

---

## Seguridad y manejo de errores

La aplicacion incluye protecciones contra:
- **Corrupcion de `config.ini`**: Si el archivo contiene un valor invalido para `bar_type`, la app carga el modelo por defecto (QTR-8RC) con aviso en stderr.
- **Acceso a array fuera de rango**: Si de algun modo se intenta inicializar con un tipo de barra invalido, la app muestra error fatal y termina limpiamente.
- **Fallos de guardado**: Si no se puede guardar `config.ini` (ej: permiso denegado), se muestra advertencia pero la sesion continua (cambios no persisten).

---

## Menu principal

El menu principal muestra el modelo de barra seleccionado, el puerto COM activo y el baudrate configurado. Desde ahi se accede a todas las funciones de la aplicacion.

---

### Opcion 1 — Seleccionar puerto

**Funcion**: `ui_select_port()` en `src/ui.c`

Detecta los puertos COM disponibles en Windows y los lista. El puerto actualmente seleccionado se marca con un asterisco (`*`). El usuario elige un nuevo puerto para la comunicacion serial. Actualiza el campo `cfg->port` en la configuracion activa.

---

### Opcion 2 — Seleccionar baudrate

**Funcion**: `ui_select_baudrate()` en `src/ui.c`

Presenta cinco velocidades fijas:
- 9600
- 19200
- 38400
- 57600
- 115200

El baudrate actual se marca con `*`. Al seleccionar uno nuevo se actualiza `cfg->baudrate`. No guarda automaticamente; usar la opcion 6 para persistir.

---

### Opcion 3 — Tipo de barra de sensores

**Funcion**: `ui_select_bar_type()` en `src/ui.c`

Lista los modelos de barra soportados:

| Modelo | Sensores | Rango de posicion | Baudrate por defecto |
|--------|----------|-------------------|----------------------|
| QTR-8RC | 8 | 0 a 7000 | 115200 |
| IM-HEM-005 | 16 | -7500 a +7500 | 115200 |

Al seleccionar un modelo:
1. Se actualiza la configuracion activa con los umbrales y baudrate por defecto del modelo seleccionado.
2. Se guarda la configuracion en `config.ini` (archivo local en disco).
3. Se reinicializa la estructura `SensorBar` en memoria (limpia datos anteriores, prepara la estructura para el nuevo tipo de barra).

**Nota**: No hay comunicacion con Arduino en este paso. Solo opera sobre datos en memoria y el archivo config.ini. La comunicacion serial comienza cuando se selecciona la opcion 5.

---

### Opcion 4 — Configurar umbrales

**Funcion**: `ui_configure_thresholds()` en `src/ui.c`

**Submenu**:
- `0` — Volver sin cambios
- `1` — Editar umbrales

Al elegir editar se pueden ajustar tres parametros (presionar Enter sin valor conserva el actual):

| Parametro | Descripcion | Valor por defecto |
|-----------|-------------|-------------------|
| Threshold BLACK | Valor >= este → sensor detectado como NEGRO | 700 |
| Threshold WHITE | Valor < este → sensor detectado como BLANCO | 300 |
| Timeout (ms) | Sin datos en este tiempo → estado TOUT | 2000 |

Los valores trabajan en escala normalizada 0-1000 (0 = blanco puro, 1000 = negro puro).

---

### Opcion 5 — Iniciar lectura / diagnostico

**Funcion**: `ui_reading_screen()` en `src/ui.c`

Requiere que haya un puerto seleccionado. Abre la conexion serial y entra en modo de monitoreo en pantalla completa.

**Controles de teclado dentro de la pantalla de lectura**:
- `Q` — Salir y volver al menu principal
- `F` — Congelar / descongelar la actualizacion del display (la lectura serial continua en segundo plano)

**Secciones del display**:

#### Encabezado
Muestra modelo de barra, puerto, baudrate e instrucciones de teclado.

#### Calibracion
- **Cal.Min**: Valores minimos recibidos durante la calibracion del Arduino (texto cyan).
- **Cal.Max**: Valores maximos recibidos durante la calibracion del Arduino (texto cyan).

#### Datos de sensores (por cada sensor)
| Columna | Descripcion |
|---------|-------------|
| Valor | Lectura cruda normalizada 0-1000 |
| Barra | Barra visual de 4 segmentos (0-25 / 25-50 / 50-75 / 75-100 %) |
| Estado | Estado clasificado con color |

**Colores de estado**:
| Estado | Color | Condicion |
|--------|-------|-----------|
| NEGRO | Verde | valor >= threshold_black |
| GRIS | Amarillo | valor entre ambos umbrales |
| BLANCO | Blanco | valor < threshold_white |
| TOUT | Rojo | sin datos dentro de timeout_ms |

#### Posicion
Valor calculado de posicion de la linea con indicacion de rango minimo/maximo.

#### Resumen de deteccion
Conteo de sensores en cada estado (Negro / Gris / Blanco).

**Comportamiento ante timeout**:
- Si no llegan datos en `timeout_ms` milisegundos el display se pone rojo y muestra "SIN DATOS".
- Todos los sensores pasan a estado TOUT.
- La aplicacion sigue esperando reconexion.

**Formato del mensaje serial esperado del Arduino**:
```
S1\tS2\t...\tSN\tposicion\n          (lecturas en vivo)
Min: min1\tmin2\t...\tminN\n         (calibracion minimos)
Max: max1\tmax2\t...\tmaxN\n         (calibracion maximos)
```

---

### Opcion 6 — Guardar configuracion

**Funcion**: `config_save()` en `src/config.c`

Escribe la configuracion activa en `config.ini` con formato INI:

```ini
[serial]
port=COM3
baudrate=115200

[bar]
bar_type=0
threshold_black=700
threshold_white=300
timeout_ms=2000
```

Muestra mensaje de exito o error. La configuracion persiste entre reinicios de la aplicacion. **La aplicacion NO guarda automaticamente al salir**; hay que usar esta opcion explicitamente.

---

### Opcion 0 — Salir

Cierra el puerto serial de forma limpia y termina la aplicacion. No guarda la configuracion automaticamente.

---

## Hardware soportado (sketches Arduino)

### QTR-8RC
- **Archivo**: `arduino/qtr8rc_visor/qtr8rc_visor.ino`
- 8 sensores en pines A0-A5, D12, D9.
- Calibracion automatica de 10 segundos al encender (indicada por LED).
- Envia calibracion min/max al inicio, luego lecturas continuas con posicion.
- Rango de posicion: 0-7000.

### IM-HEM-005
- **Archivo**: `arduino/im_hem_005/im_hem_005.ino`
- 16 sensores via multiplexor 4-bit (pines A0-A3 seleccion, A4 lectura).
- Calibracion manual con boton en D2 (colocar sobre negro, luego sobre blanco).
- LED parpadea durante calibracion, queda fijo al transmitir.
- Rango de posicion: -7500 a +7500 (suma ponderada; izquierda negativo, derecha positivo).

---

## Flujo de datos

```
Arduino (sensor bar)
    │  Serial COM (tab-delimited text)
    ▼
serial.c  ──►  sensor_bar.c (parseo)  ──►  diagnostic.c (clasificacion)
                                                    │
                                                    ▼
                                             ui.c (display en consola)
```

La configuracion se carga desde `config.ini` al inicio via `config_load()` y se puede guardar en cualquier momento con la opcion 6 del menu.

# Para compilar en powershell
```sh
cd "C:\Cerea_K\visor_console"; gcc -Wall -Wextra -std=c11 -Isrc -c src/serial.c -o src/serial.o; gcc -Wall -Wextra -std=c11 -Isrc -c src/ui.c -o src/ui.o; gcc src/main.o src/serial.o src/sensor_bar.o  src/diagnostic.o src/ui.o src/config.o src/utils.o -o visor.exe
```