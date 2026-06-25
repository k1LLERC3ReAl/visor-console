# Plan: Nueva arquitectura — Visor Console con upload/restore de firmware

## Contexto

El usuario quiere transformar Visor Console de un simple monitor serial en una herramienta completa de diagnóstico que también controla el firmware del microcontrolador. El flujo nuevo es:

1. App detecta puerto COM automáticamente
2. **Backup** del firmware actual del Arduino (hex binario)
3. Usuario selecciona baudrate y barra de sensores
4. App modifica el `.ino` con el baudrate elegido, compila y sube el sketch correspondiente
5. Usuario corre el diagnóstico normalmente
6. Al salir, app **restaura** el firmware original (hex del backup)

Esto requiere integrar `arduino-cli` (compilar + subir) y `avrdude` (backup/restore de hex).

---

## Dependencias externas a instalar

| Herramienta | Uso | Instalación |
|-------------|-----|-------------|
| `arduino-cli` | Compilar .ino y subir firmware | Descargar de arduino.cc/arduino-cli |
| `avrdude` | Backup y restore de hex | Viene con arduino-cli o con Arduino IDE |

Paths configurables en `config.ini` (sección `[tools]`). Si no se encuentran, la app los busca en PATH.

---

## Cambios al modelo de datos

### `AppConfig` (config.h / config.c)
Agregar campos:
```c
char board_fqbn[64];        // ej: "arduino:avr:nano:cpu=atmega328old"
char arduino_cli_path[256]; // path a arduino-cli.exe, default "arduino-cli"
char avrdude_path[256];     // path a avrdude.exe, default "avrdude"
char backup_hex_path[256];  // donde guardar el hex backup, default "backup_fw.hex"
```

### `config.ini` — nueva sección
```ini
[tools]
board_fqbn=arduino:avr:nano:cpu=atmega328old
arduino_cli_path=arduino-cli
avrdude_path=avrdude
backup_hex_path=backup_fw.hex
```

### Lista de boards predefinidas (nuevo archivo `src/boards.h`)
```c
typedef struct { char name[48]; char fqbn[64]; } BoardDef;
extern const BoardDef KNOWN_BOARDS[];
extern const int KNOWN_BOARD_COUNT;
```
Boards iniciales: Nano (old bootloader), Nano (new bootloader), Uno, Mega 2560.

---

## Nuevo módulo: `src/upload.c` / `src/upload.h`

```c
// Backup del firmware actual via avrdude
int upload_backup_firmware(const char *avrdude, const char *port,
                           const char *fqbn, const char *out_hex);

// Restaurar firmware desde hex via avrdude
int upload_restore_firmware(const char *avrdude, const char *port,
                            const char *fqbn, const char *hex_path);

// Modificar Serial.begin(XXXX) en copia temporal del .ino
int upload_patch_baudrate(const char *src_ino, const char *dst_ino, int baudrate);

// Compilar y subir sketch via arduino-cli
int upload_flash_sketch(const char *arduino_cli, const char *sketch_dir,
                        const char *fqbn, const char *port);
```

Cada función ejecuta el proceso externo con `CreateProcess` (Windows API) y captura stdout/stderr para mostrar progreso al usuario.

---

## Cambios en UI (`src/ui.c`)

### Nuevo menú principal
```
1. Seleccionar puerto COM
2. Seleccionar baudrate
3. Seleccionar tipo de placa (board)      <- NUEVO
4. Seleccionar barra de sensores
5. Configurar umbrales
6. Iniciar diagnóstico                    <- flujo expandido
7. Guardar configuración
0. Salir
```

### Nuevo `ui_select_board()` (opción 3)
Lista de boards predefinidas con opción de ingresar FQBN manual.

### `ui_reading_screen()` modificado — flujo completo de opción 6
```
[1] Backup firmware actual (avrdude)        -> "Guardando firmware original..."
[2] Patch baudrate en copia temporal del .ino
[3] Compilar y subir sketch (arduino-cli)   -> progreso en texto
[4] Pausa 2s para que Arduino reinicie
[5] Abrir serial y correr monitor (código existente, sin cambios internos)
[6] Al salir (Q): cerrar serial
[7] Restaurar firmware original (avrdude)   -> "Restaurando firmware anterior..."
```

Si el backup falla → advertir y pedir confirmación antes de continuar.
Si la restauración falla → mostrar el path del hex para que el usuario lo flashee manualmente.

---

## Archivos a crear / modificar

| Archivo | Acción |
|---------|--------|
| `src/upload.c` | Crear — módulo nuevo |
| `src/upload.h` | Crear |
| `src/boards.h` | Crear — lista de FQBNs predefinidos |
| `src/config.h` | Modificar — agregar campos de tools y board |
| `src/config.c` | Modificar — load/save de nuevos campos |
| `src/ui.c` | Modificar — nuevo menú, `ui_select_board()`, flujo opción 6 |
| `src/ui.h` | Modificar — declarar `ui_select_board()` |
| `src/main.c` | Modificar — agregar case para opción 3, ajustar numeración |
| `Makefile` | Modificar — agregar `src/upload.c` a SOURCES |
| `CLAUDE.md` | Actualizar — nueva arquitectura, dependencias, flujo |

---

## Fix pendiente (aprovechar este ciclo)

Corregir `arduino/im_hem_005/im_hem_005.ino` línea 146:
```c
// Cambiar:
Serial.begin(9600);
// Por:
Serial.begin(115200);
```
(El plan.md marca esto como "hecho" pero el archivo en disco todavía tiene 9600.)

---

## Verificación

1. **Sin hardware**: confirmar que backup/restore llaman a avrdude con los args correctos (modo dry-run con `echo`).
2. **Con Arduino Nano**: conectar, ejecutar opción 6, verificar:
   - Se crea `backup_fw.hex` en disco
   - El sketch de la barra elegida se compila y sube (Arduino reinicia, LED de calibración activa)
   - El monitor serial muestra datos
   - Al presionar Q, se restaura el firmware original y el Arduino vuelve al comportamiento previo
3. **Fallo de avrdude**: desconectar Arduino a mitad del proceso, verificar que el error se muestra y el path del hex queda disponible.
