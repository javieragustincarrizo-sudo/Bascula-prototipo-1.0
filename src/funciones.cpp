#include "funciones.h"

float FACTOR_DIVISOR = 2.0; //Utilizo 2 porque uso dos resistencias iguales y el valor del divisor divide a la mitad

uint8_t lecturaBateria(void){

  uint32_t suma_miliVoltios = 0;
  const int MUESTRAS = 20;
  digitalWrite(ADC_CONTROL, HIGH); // Activamos el divisor de voltaje para la lectura de la batería
  // Tomamos múltiples muestras para eliminar el ruido analógico
  for (int i = 0; i < MUESTRAS; i++) {
    suma_miliVoltios += analogReadMilliVolts(PIN_BATERIA);
    delay(5); // Pequeña pausa entre lecturas
  }
  digitalWrite(ADC_CONTROL, LOW); // Desactivamos el divisor de voltaje para la lectura de la batería
  
  float promedio_pin_mV = (float)suma_miliVoltios / MUESTRAS;
  
  // Convertimos a Voltios reales de la batería
  float voltaje_bateria = (promedio_pin_mV * FACTOR_DIVISOR) / 1000.0;
  
  // Mapeo preciso para una celda 18650 (4.2V = 100%, 3.0V = 0%)
  float porcentaje = ((voltaje_bateria - 3.0) / (4.2 - 3.0)) * 100.0;
  
  // Restringir los valores entre 0 y 100
  if(porcentaje > 100.0) porcentaje = 100.0;
  if(porcentaje < 0.0) porcentaje = 0.0;

  //porcentaje = 42; /**************************************************************VALOR FIJO PARA VER EN OLED- QUITAR*/
  return (uint8_t)porcentaje;

}

void ejecutarCalibracionLocal(float pesoConocido) {
    Serial.print("Calibrando con peso patrón de: ");
    Serial.print(pesoConocido);
    Serial.println(" kg...");

    // 1. Obtener la lectura analógica directa (cruda) de la celda de carga actual (promedio de 10 muestras)
    // get_value() entrega la lectura cruda restando el valor de la Tara actual
    double lectura_cruda = scale.get_value(10); 

    // 2. Calcular el nuevo factor de escala
    if (lectura_cruda != 0) {
        factor_calibracion = lectura_cruda / pesoConocido;
        
        // 3. Aplicar el nuevo factor inmediatamente al objeto del sensor
        scale.set_scale(factor_calibracion);
        
        // 4. Guardar permanentemente en la memoria Flash NVS del ESP32
        prefs.begin("balanza_cfg", false); // Abrir en modo escritura
        prefs.putFloat("facCal", factor_calibracion);
        prefs.end();
        
        Serial.print("¡Calibrado! Nuevo factor guardado en Flash: ");
        Serial.println(factor_calibracion);
    } else {
        Serial.println("Error: Lectura cruda en cero. ¿Colocó el peso patrón?");
    }
}

void ejecutarTaraLocal() {
    scale.tare();
    Serial.println("Comando Tara ejecutado en HX711.");
}