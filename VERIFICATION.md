# Verificacion de correcion: Manejo de errores en bar_type

## Descripcion del bug corregido

La aplicacion fallaba (crash) cuando se encontraba un valor invalido para `bar_type` en `config.ini`. Las causas eran:

1. `src/config.c:48` no validaba el rango de `bar_type` al leerlo del archivo
2. `src/sensor_bar.c:33` accedia a `SENSOR_BAR_MODELS[type]` sin validar el indice

## Cambios realizados

| Archivo | Cambio |
|---------|--------|
| `src/sensor_bar.c` | Agregado guard al inicio de `sensor_bar_init()` que valida 0 <= type < BAR_COUNT |
| `src/config.c` | Validacion de rango para `bar_type` al cargar config.ini; usa default si es invalido |
| `src/ui.c` | Verificacion del retorno de `config_save()` en `ui_select_bar_type()` |
| `src/sensor_bar.c` | Agregado `#include <stdio.h>` para soporte de fprintf/exit |

## Pruebas de verificacion

### Prueba 1: Config corrupta con bar_type invalido (el caso reportado)

```bash
# Copiar el archivo de test
copy config.ini.test config.ini

# Ejecutar la app
visor.exe
```

**Resultado esperado**:
- El programa debe imprimir en stderr (ventana de error de Windows o consola):
  ```
  Error: bar_type=5 en config.ini es invalido (rango valido: 0-1).
  Se usara el tipo por defecto (QTR-8RC).
  ```
- La aplicacion debe cargar normalmente con QTR-8RC como modelo
- El menu principal debe mostrar "Barra : QTR-8RC"

### Prueba 2: Seleccionar IM-HEM-005 desde el menu (flujo normal)

```bash
# Restaurar config.ini original o dejarla vacia
rm config.ini

# Ejecutar
visor.exe

# En el menu: opcion 3
# Seleccionar: 2 (IM-HEM-005)
```

**Resultado esperado**:
- El programa debe guardar `config.ini` con `bar_type=1`
- El menu debe mostrar "Barra : IM-HEM-005"
- Sin errores ni avisos

### Prueba 3: Fallo de escritura en config.ini

```bash
# (En Windows) Quitar permiso de escritura al directorio
# Ejecutar visor.exe
# Opcion 3: cambiar modelo

# (O crear config.ini como read-only)
attrib +r config.ini

visor.exe

# Opcion 3: cambiar modelo
```

**Resultado esperado**:
- Se muestra advertencia:
  ```
  Advertencia: no se pudo guardar la configuracion en config.ini.
  Los cambios se aplicaron en esta sesion pero no persisten.
  ```
- El programa pausa esperando Enter
- Los cambios no se guardan en el archivo

### Prueba 4: Valor negativo o muy grande en config.ini

```bash
# Editar config.ini manualmente
echo bar_type=-1 >> config.ini

visor.exe
```

**Resultado esperado**:
- Error en stderr (igual que Prueba 1)
- App carga con QTR-8RC por defecto

---

## Conclusión

El bug fue causado por falta de validacion de rango de `bar_type`. La correccion:

1. **Valida en dos puntos** (defense in depth):
   - `config.c` al cargar el archivo (detecta corrupcion)
   - `sensor_bar.c` al inicializar (detecta cualquier otro camino que pase un valor invalido)

2. **Diferencia entre errores**:
   - Carga de config: aviso + default (el usuario no pierde otros datos)
   - Init directo: error fatal (indica corrupcion o bug interno)

3. **Reporta errores al usuario**:
   - Mensajes descriptivos en stderr
   - Advertencias en stdout cuando es recuperable
   - Exit code 1 cuando es fatal
