# Plan: Problema 3 - Opciones de corrección para has_calibration

## Contexto

Con los cambios ya realizados (Problema 1 y 2):
1. ✅ **Baudrate corregido**: Arduino ahora usa 115200 (igual que la app)
2. ✅ **Formato de calibración**: Arduino ahora envía `Min:\t...` y `Max:\t...` en formato correcto

El Problema 3 ahora se **resuelve automáticamente** porque el parser en `src/sensor_bar.c:75-100` encontrará los datos y ejecutará:
```c
if (is_max) bar->has_calibration = 1;  // Se activa al recibir Max:
```

**Sin embargo**, hay escenarios donde el flag no se activa correctamente:

1. **Arduino envía datos pero solo "Min:" llega** (línea de comunicación cortada)
2. **Datos llegan corrompidos** y el parser no los reconoce
3. **Timeout en recepción** de calibración
4. **Usuario quiere acceder a pantalla de lectura sin esperar calibración**

---

## 5 Opciones de Corrección para Mayor Robustez

### **OPCIÓN A: Mínimo cambio (RECOMENDADO - Hacer primero)**
**Descripción**: Confiar en que el Problema 2 está resuelto y el parser funcionará.

**Cambios en C**: NINGUNO (ya funciona con los cambios del Arduino)

**Cambios en Arduino**: Ya realizados (baudrate 115200 + envío "Min:" y "Max:")

**Ventajas**:
- Simple, sin cambios adicionales
- Si Arduino envía correctamente, funciona perfecto

**Desventajas**:
- Sin protección si datos llegan incompletos o corrompidos
- Usuario queda esperando indefinidamente en pantalla de "Calibrando..."

**Cuándo usar**: Como primer paso. Probar con hardware real y validar que los datos llegan completos.

---

### **OPCIÓN B: Timeout para calibración**
**Descripción**: Si no recibe "Max:" dentro de X segundos, mostrar error y permitir continuar.

**Cambios en C**:
- En `src/ui.c:ui_reading_screen()`, agregar variable `time_t calibration_start` 
- Después de línea 340 (al entrar a la pantalla), iniciar `calibration_start = utils_millis()`
- En el loop principal (línea 356-370), agregar:
```c
if (!bar->has_calibration && !frozen) {
    if ((int)(utils_millis() - calibration_start) > 30000) {  // 30s timeout
        // Mostrar: "Timeout en calibración, continuando sin datos de cal..."
        bar->has_calibration = 1;  // Forzar mostrar pantalla normal
    }
}
```

**Ventajas**:
- Usuario no queda colgado esperando indefinidamente
- Permite diagnosticar problemas de comunicación
- Educativo: el usuario sabe que algo no funcionó

**Desventajas**:
- Requiere nueva variable de estado
- Timeout arbitrario (¿30s? ¿60s?)

---

### **OPCIÓN C: Mostrar estado intermedio**
**Descripción**: Cuando recibe "Min:" pero falta "Max:", mostrar "Calibración parcial" en lugar de "Calibrando..."

**Cambios en C**:
- Agregar flag en `SensorBar` struct (`src/sensor_bar.h`):
```c
int has_calibration_min_only;  // Se activa con Min:, se reseta con Max:
```

- En `src/sensor_bar.c:sensor_bar_parse_calibration()`, después de procesar "Min:":
```c
if (is_min) bar->has_calibration_min_only = 1;
if (is_max) bar->has_calibration_min_only = 0;  // Se reseta cuando llega Max
```

- En `src/ui.c:ui_reading_screen()` líneas 395-404, mostrar tres estados:
  - "Calibrando sensores..." (sin datos)
  - "Calibración parcial (Min)..." (solo Min recibido)
  - "Calibración completa. Esperando primera lectura..." (Min + Max)

**Ventajas**:
- Usuario ve progreso en tiempo real
- Útil para diagnosticar problemas de comunicación
- No requiere timeout arbitrario

**Desventajas**:
- Más complejidad en el código
- Requiere cambios en struct y display

---

### **OPCIÓN D: Permitir continuar sin calibración (con advertencia)**
**Descripción**: Usuario puede presionar 'C' para continuar sin calibración, o esperar.

**Cambios en C**:
- En `src/ui.c:ui_reading_screen()` línea 348-351, agregar tecla 'C':
```c
if (c == 'c' || c == 'C') has_valid_data = 1;  // Skip calibration wait
```

- Mostrar en pantalla: `"[Q] Salir [F] Freeze [C] Continuar sin calibración"`

**Ventajas**:
- Máxima flexibilidad para el usuario
- Permite diagnosticar si el problema es Arduino vs app
- Útil en desarrollo y debugging

**Desventajas**:
- Usuario necesita saber qué hace cada tecla
- Puede llevar a uso incorrecto (sin calibración real)

---

### **OPCIÓN E: Mostrar calibración apenas llega (mejor UX)**
**Descripción**: Mostrar valores de calibración (incluso solo Min) en tiempo real en la pantalla de "calibrando...", actualizarlos cuando llega Max.

**Cambios en C**:
- Modificar `src/ui.c` líneas 395-406 para mostrar valores de calibración incluso si solo "Min:" llegó
- En lugar de esperar `has_calibration = 1`, usar `has_calibration_min_only`
- Mostrar en pantalla: "Min:" valores cuando llegan, "Max:" valores cuando llegan

**Ventajas**:
- Mejor feedback visual al usuario
- Se ve que la comunicación funciona
- Educativo

**Desventajas**:
- Cambios significativos en display logic
- Requiere nueva lógica de estado

---

## Recomendación

**Ejecutar en este orden**:

1. **Ahora**: Probar OPCIÓN A (ya está implementada)
   - Cargar código en Arduino
   - Conectar hardware
   - Verificar que calibración aparece correctamente

2. **Si funciona**: Documentar y dejar como está

3. **Si hay problemas**: Implementar **OPCIÓN C** (estado intermedio)
   - Es la que proporciona mejor diagnóstico
   - Requiere cambios mínimos adicionales
   - Ayuda al usuario a entender qué está pasando

4. **Si requiere flexibilidad avanzada**: Agregar OPCIÓN D (tecla C para continuar)

---

## Cambios ya realizados (en Arduino)

```c
// Línea 187: Baudrate
Serial.begin(115200);  // ← CAMBIADO de 9600

// Líneas 128-145: Envío de calibración
Serial.print("Min: ");
for (int i = 0; i < NUM_SENSORES; i++) {
    Serial.print(calMin[i]);
    if (i < NUM_SENSORES - 1) Serial.print("\t");
}
Serial.println();

Serial.print("Max: ");
for (int i = 0; i < NUM_SENSORES; i++) {
    Serial.print(calMax[i]);
    if (i < NUM_SENSORES - 1) Serial.print("\t");
}
Serial.println();
```

---

## Próximos pasos

1. Cargar sketch en Arduino
2. Ejecutar `visor.exe` con IM-HEM-005
3. Realizar calibración
4. Observar si "Calibración recibida" aparece en pantalla
5. Si no aparece → implementar OPCIÓN C
