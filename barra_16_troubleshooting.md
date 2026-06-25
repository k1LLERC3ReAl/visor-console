# Barra IM-HEM-005 - Troubleshooting de Calibración

## Problema Identificado

El código C no recibe los datos de calibración (`Min:` y `Max:`) del Arduino porque:

1. **El Arduino envía la calibración una sola vez al inicio** durante su `setup()`
2. **El código C se conecta después**, cuando el Arduino ya terminó de enviar
3. **Resultado**: El código C solo recibe datos de sensores continuos, nunca recibe `Min:` y `Max:`

El archivo `output_arduino.txt` confirma que el Arduino **SÍ envía correctamente**:
```
Min: 993	979	868	752	644	531	479	424	394	424	427	486	483	456	548	626
Max: 987	992	986	989	989	983	993	989	988	982	989	994	1000	996	1000	1013
```

---

## Soluciones Propuestas

### Opción 1: Comando para Re-solicitar Calibración ⭐ RECOMENDADO

**Cómo funciona:**
1. El código C envía un comando al Arduino (ej: `"SEND_CALIB\n"`)
2. El Arduino recibe ese comando y re-envía los datos de calibración
3. El código C recibe "Min:" y "Max:" y los parsea

**Ventajas:**
- ✅ Simple de implementar
- ✅ El Arduino siempre tiene la calibración disponible en memoria
- ✅ Si el usuario reconnecta, puede pedir la calibración de nuevo
- ✅ El código C controla cuándo quiere recibir la calibración
- ✅ Determinista y confiable
- ✅ Fácil de debuggear

**Desventajas:**
- ❌ Requiere lógica bidireccional (C envía, Arduino responde)
- ❌ Necesita protocolo de comunicación más complejo
- ❌ Si hay error, el código C debe reintentar

**Flujo:**
```
Código C → envía "SEND_CALIB" → Arduino recibe → Arduino envía Min/Max
```

**Implementación en Arduino:**
```cpp
void loop() {
  // Leer línea del serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "SEND_CALIB") {
      // Re-enviar calibración
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
    }
  }
  
  // ... resto del código ...
}
```

---

### Opción 2: Almacenar Calibración y Enviar Automáticamente

**Cómo funciona:**
1. Arduino almacena la calibración en memoria
2. Cada vez que se detecta una nueva conexión (reset del puerto), Arduino **automáticamente** envía los datos
3. No necesita comando, es automático

**Ventajas:**
- ✅ Sin protocolo, más simple conceptualmente
- ✅ Automático: código C solo recibe sin tener que pedir
- ✅ Funciona incluso con otros programas (Arduino IDE, PuTTY, etc.)
- ✅ Si Arduino se reinicia, automáticamente re-envía calibración

**Desventajas:**
- ❌ Arduino siempre envía calibración al conectar (puede ser molesto)
- ❌ Si hay problemas de conexión, puede saturar el buffer
- ❌ Más difícil de debuggear
- ❌ Necesita detectar nueva conexión (reset del puerto COM)
- ❌ Requiere lógica de estado más compleja

**Flujo:**
```
Arduino inicia → detecta conexión → envía Min/Max automáticamente
Código C conecta → recibe Min/Max sin pedir
```

---

## Comparación Rápida

| Aspecto | Opción 1 (Comando) | Opción 2 (Automático) |
|---------|------------------|----------------------|
| **Complejidad** | Media | Media-Alta |
| **Confiabilidad** | Alta (C controla) | Media (automático) |
| **Facilidad** | Requiere protocolo | Requiere detección conexión |
| **Compatibilidad** | Solo funciona con C | Funciona con cualquier serial |
| **Debugging** | Más fácil | Más difícil |
| **Determinismo** | Determinista | No determinista |

---

## Recomendación Final

**→ Usar Opción 1 (Comando)**

### Razones:
1. **Control explícito**: El código C solicita cuando realmente necesita la calibración
2. **Confiabilidad**: Puedes reintentar si falla
3. **Debugging**: Es más fácil ver qué está pasando
4. **Mantenibilidad**: Protocolo claro y simple
5. **Escalabilidad**: Puedes agregar más comandos después

### Pasos de Implementación:

1. **Modificar Arduino**: Agregar handler para comando `SEND_CALIB`
2. **Modificar Código C**: Enviar `SEND_CALIB` al conectar
3. **Compilar y probar**: Verificar que `Min:` y `Max:` llegan correctamente
4. **Remover debug statements**: Limpiar los `[DEBUG serial_read_line]` y `[SERIAL RX]` cuando todo funcione

---

## Estado Actual del Debugging

El código C ya tiene debugging agregado:
- `[SERIAL RX len=X] CONTENIDO:` - Muestra cada línea recibida
- `[DEBUG serial_read_line]` - Muestra detalles de lectura de puerto
- `[DEBUG CALIB]` - Muestra detalles de parseo de calibración

Estos pueden ser removidos una vez que la solución esté funcionando.

---

## Próximos Pasos

1. ¿Implementamos la Opción 1?
2. Modificar código Arduino para recibir comando `SEND_CALIB`
3. Modificar código C para enviar ese comando
4. Compilar y probar
5. Remover debug statements cuando todo funcione
