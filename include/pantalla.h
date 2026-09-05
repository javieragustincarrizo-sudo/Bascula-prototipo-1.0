#ifndef PANTALLA_H
#define PANTALLA_H

#include <Arduino.h>

// Variables que requiere tu lógica de visualización (del flujo anterior)
extern const char* ssidMaster;
extern const char* passMaster;

// Declaración de funciones de pantalla
void inicializarPantalla();
void mostrarQR_ConexionWiFi();
void mostrarQR_MAC(String mac);
void actualizarPantallaLocal(String textoRol); // <-- AGREGAR ESTA LÍNEA
void drawScreen();
#endif
