/*
 * DEBUG BARRA IM-16 (QRE1113 × 74HC4067)
 * SOLO SENSORES - Sin motores, sin RRT
 *
 * Hardware:
 *   Arduino Nano
 *   Barra IM-16: SL0→A0, SL1→A1, SL2→A2, SL3→A3, OM→A4, LON→GND, +5V→VCC, GND→GND
 *   Botón calibración: D2 (INPUT_PULLUP)
 *   LED indicador: D13
 */

// ---------- Pines barra IM-16 ----------
#define PIN_SL0  A0
#define PIN_SL1  A1
#define PIN_SL2  A2
#define PIN_SL3  A3
#define PIN_OM   A4

#define NUM_SENSORES 16

// ---------- Botón y LED ----------
#define PIN_BOTON  2
#define LED 13

// ---------- Calibración ----------
int calMin[NUM_SENSORES];
int calMax[NUM_SENSORES];

// ===================== LECTURA MUX =====================

int leerSensor(int canal) {
  digitalWrite(PIN_SL0, (canal >> 0) & 1);
  digitalWrite(PIN_SL1, (canal >> 1) & 1);
  digitalWrite(PIN_SL2, (canal >> 2) & 1);
  digitalWrite(PIN_SL3, (canal >> 3) & 1);
  delayMicroseconds(10);
  return analogRead(PIN_OM);
}

void leerTodos(int* valores) {
  for (int i = 0; i < NUM_SENSORES; i++) {
    valores[i] = leerSensor(i);
  }
}

// ===================== ESPERAR BOTÓN CON LED PARPADEANDO =====================

void esperarBoton(const char* mensaje) {
  Serial.print(mensaje);
  Serial.println(" (presiona botón)");
  
  // Espera a que se suelte el botón
  while (digitalRead(PIN_BOTON) == LOW) {
    digitalWrite(LED, (millis() / 300) % 2);
    delay(10);
  }
  delay(50);
  
  // Espera a que se presione
  while (digitalRead(PIN_BOTON) == HIGH) {
    digitalWrite(LED, (millis() / 300) % 2);
    delay(10);
  }
  delay(50);
  
  Serial.println("→ Botón presionado");
}

// ===================== CALIBRACIÓN =====================

void calibrar() {
  int valores[NUM_SENSORES];

  Serial.println("\n");
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║    INICIANDO CALIBRACIÓN       ║");
  Serial.println("╚════════════════════════════════╝");
  delay(500);

  // --- NEGRO ---
  esperarBoton("PASO 1: Posiciona sobre línea NEGRA");
  
  Serial.print("Leyendo NEGRO... ");
  digitalWrite(LED, HIGH);
  delay(200);
  leerTodos(valores);
  for (int i = 0; i < NUM_SENSORES; i++) {
    calMax[i] = valores[i];
  }
  Serial.println("✓ OK\n");
  
  Serial.println("Valores NEGRO (máximo):");
  for (int i = 0; i < NUM_SENSORES; i++) {
    Serial.print("S");
    if (i < 10) Serial.print("0");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(calMax[i]);
    if ((i + 1) % 4 == 0) Serial.println();
    else Serial.print("  |  ");
  }
  Serial.println("\n");
  delay(1000);

  // --- BLANCO ---
  esperarBoton("PASO 2: Posiciona sobre línea BLANCA");

  Serial.print("Leyendo BLANCO... ");
  digitalWrite(LED, HIGH);
  delay(200);
  leerTodos(valores);
  for (int i = 0; i < NUM_SENSORES; i++) {
    calMin[i] = valores[i];
  }
  Serial.println("✓ OK\n");
  
  Serial.println("Valores BLANCO (mínimo):");
  for (int i = 0; i < NUM_SENSORES; i++) {
    Serial.print("S");
    if (i < 10) Serial.print("0");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(calMin[i]);
    if ((i + 1) % 4 == 0) Serial.println();
    else Serial.print("  |  ");
  }
  Serial.println("\n");

  Serial.println("╔════════════════════════════════╗");
  Serial.println("║    CALIBRACIÓN COMPLETA ✓      ║");
  Serial.println("╚════════════════════════════════╝");
  Serial.println("\nIniciando lectura continua...\n");
  
  digitalWrite(LED, LOW);
  delay(1000);
}

// ===================== LECTURA DE POSICIÓN =====================

int leerPosicion() {
  int valores[NUM_SENSORES];
  leerTodos(valores);

  long suma_pond = 0;
  long suma_val  = 0;

  for (int i = 0; i < NUM_SENSORES; i++) {
    int v = map(valores[i], calMin[i], calMax[i], 0, 1000);
    v = constrain(v, 0, 1000);

    suma_pond += (long)v * (i * 1000);
    suma_val  += v;
  }

  if (suma_val < 500) return -1;

  return (int)(suma_pond / suma_val);
}

// ===================== MOSTRAR GRÁFICO DE SENSORES =====================

void mostrarGrafico(int* valores) {
  for (int i = 0; i < NUM_SENSORES; i++) {
    int normalizado = map(valores[i], calMin[i], calMax[i], 0, 100);
    normalizado = constrain(normalizado, 0, 100);
    
    Serial.print("S");
    if (i < 10) Serial.print("0");
    Serial.print(i);
    Serial.print(": [");
    
    // Barra visual
    for (int j = 0; j < 20; j++) {
      if (j < normalizado / 5) Serial.print("█");
      else Serial.print("░");
    }
    
    Serial.print("] ");
    Serial.print(normalizado);
    Serial.println("%");
  }
}

// ===================== SETUP =====================

void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║   DEBUG BARRA IM-16 (16x)      ║");
  Serial.println("║   Ingeniero Maker              ║");
  Serial.println("╚════════════════════════════════╝\n");

  // Configurar pines
  pinMode(PIN_SL0, OUTPUT);
  pinMode(PIN_SL1, OUTPUT);
  pinMode(PIN_SL2, OUTPUT);
  pinMode(PIN_SL3, OUTPUT);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Serial.println("✓ Pines configurados");
  delay(500);

  // Calibración
  calibrar();
}

// ===================== LOOP =====================

void loop() {
  int valores[NUM_SENSORES];
  int posicion = leerPosicion();

  // Leer todos los sensores
  leerTodos(valores);

  // Limpiar pantalla (opcional, comentar si no quieres)
  Serial.write(27);
  Serial.print("[2J");
  Serial.write(27);
  Serial.print("[H");

  // Mostrar posición
  Serial.println("╔════════════════════════════════╗");
  Serial.print("║ POSICIÓN: ");
  if (posicion == -1) {
    Serial.print("   SIN LÍNEA       ");
  } else {
    Serial.print(posicion);
    for (int i = 0; i < 15 - String(posicion).length(); i++) Serial.print(" ");
  }
  Serial.println("║");
  Serial.println("╠════════════════════════════════╣");

  // Mostrar gráfico
  mostrarGrafico(valores);

  Serial.println("╠════════════════════════════════╣");
  Serial.println("║ Valores RAW:                   ║");
  Serial.println("╠════════════════════════════════╣");
  
  for (int i = 0; i < NUM_SENSORES; i++) {
    Serial.print("S");
    if (i < 10) Serial.print("0");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(valores[i]);
    Serial.print(" (");
    int normalizado = map(valores[i], calMin[i], calMax[i], 0, 1000);
    normalizado = constrain(normalizado, 0, 1000);
    Serial.print(normalizado / 10);
    Serial.println("%)");
  }
  
  Serial.println("╚════════════════════════════════╝\n");

  delay(500);
}
