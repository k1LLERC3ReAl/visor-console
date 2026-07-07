# Visor Console

Herramienta de diagnóstico para Windows que se conecta a barras de sensores de seguimiento de línea (line-following) montadas en Arduino, sube un sketch de diagnóstico y muestra las lecturas de sensores en tiempo real por consola.

Incluye además una interfaz gráfica de escritorio (Electron) como alternativa a la consola.

## Qué hace

1. Hace backup del firmware que esté cargado en el Arduino antes de tocar nada.
2. Compila y sube un sketch de diagnóstico con el baudrate elegido.
3. Muestra en pantalla las lecturas de cada sensor y su clasificación (NEGRO / GRIS / BLANCO / TOUT).
4. Restaura automáticamente el firmware original al finalizar.

## Hardware soportado

| Barra | Sensores | Rango de posición |
|---|---|---|
| QTR-8RC | 8 (analógicos) | 0–7000 |
| IM-HEM-005 | 16 (vía MUX 4-bit) | −7500 a +7500 |

Boards Arduino predefinidas: Nano (bootloader viejo/nuevo), Uno, Mega, o un FQBN personalizado.

## Requisitos

- Windows 10+ (usa la API de consola con ANSI/UTF-8)
- [`arduino-cli`](https://arduino.github.io/arduino-cli/) — compilar y subir sketches
- `avrdude` — backup/restore del firmware

Ambas herramientas deben estar en el `PATH`, o configuradas explícitamente en `config.ini`.

## Compilar (consola)

```powershell
gcc -Wall -Wextra -std=c11 -Isrc -c src/main.c           -o src/main.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/serial.c         -o src/serial.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/sensor_bar.c     -o src/sensor_bar.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/diagnostic.c     -o src/diagnostic.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/ui.c             -o src/ui.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/config.c         -o src/config.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/utils.c          -o src/utils.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/boards.c         -o src/boards.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/upload.c         -o src/upload.o
gcc src/main.o src/serial.o src/sensor_bar.o src/diagnostic.o src/ui.o src/config.o src/utils.o src/boards.o src/upload.o -o visor.exe
```

O simplemente:

```powershell
make
```

Ejecutar:

```powershell
.\visor.exe
```

## Interfaz gráfica (Electron)

```powershell
cd app
npm install
npm start
```

## Configuración

La app lee `config.ini` en el directorio de trabajo (no versionado, se genera/edita desde el menú de la app):

```ini
[serial]
port=COM3
baudrate=115200

[bar]
bar_type=0
threshold_black=700
threshold_white=300
timeout_ms=2000

[tools]
board_fqbn=arduino:avr:nano:cpu=atmega328old
arduino_cli_path=arduino-cli
avrdude_path=avrdude
backup_hex_path=backup_fw.hex
```

`config.ini` no se guarda automáticamente al salir — hay que usar la opción 7 (Guardar) del menú.

## Sketches Arduino

- `arduino/qtr8rc_visor/qtr8rc_visor.ino` — QTR-8RC, 8 sensores (A0–A5 + D12 + D9), 115200 baud, auto-calibración de 10s.
- `arduino/im_hem_005/im_hem_005.ino` — IM-HEM-005, 16 sensores vía MUX 4-bit (A0–A4), 115200 baud, calibración manual por botón.

## Estructura del proyecto

```
src/
├── main.c        Loop principal del menú
├── serial.c/.h   I/O serial vía Windows API (CreateFile, SetCommState, ReadFile)
├── sensor_bar.c/.h   Parseo de lecturas y calibración
├── diagnostic.c/.h   Clasificación por sensor contra umbrales
├── ui.c/.h       Menús de consola y pantalla de lectura
├── config.c/.h   Carga/guardado de config.ini
├── boards.c/.h   FQBNs predefinidos de boards Arduino
├── upload.c/.h   Wrapper de arduino-cli / avrdude
└── utils.c/.h    Utilidades de timing

arduino/          Sketches de diagnóstico para cada barra
app/              Interfaz gráfica de escritorio (Electron)
```

Ver [CLAUDE.md](CLAUDE.md) para el detalle del protocolo serial y el flujo de diagnóstico completo.

## Licencia

MIT — ver [LICENSE](LICENSE).

## Repositorio

https://github.com/k1LLERC3ReAl/visor-console
