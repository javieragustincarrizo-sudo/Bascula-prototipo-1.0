#ifndef FUNCIONES_H
#define FUNCIONES_H
#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
extern Preferences prefs;

#define PIN_BATERIA 0
#define ADC_CONTROL 1

// En funciones.h (Líneas 10 y 11)
extern HX711 scale; 
extern float factor_calibracion;

uint8_t lecturaBateria();
void ejecutarCalibracionLocal(float pesoConocido);
void ejecutarTaraLocal();

#endif