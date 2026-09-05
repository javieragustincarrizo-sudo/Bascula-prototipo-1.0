#include <qrcode.h>
#include "comunicaciones.h"
#include "qrcode.h" // <-- Siempre primero y local
#include "pantalla.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "funciones.h"


#ifdef USAR_PANTALLA_OLED

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Compartido con el reset del ESP32
#define i2c_Address 0x3C

extern float pesoActual;
const char* ssidMaster = "Bascula_Master_AP";
const char* passMaster = "123456789";


Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);



void inicializarPantalla() {
  Wire.begin(8, 10); 
  if(!display.begin(i2c_Address, true)) {
    Serial.println("OLED no encontrado");
    //for(;;);
  }
  display.display();
  delay(1000);
  display.clearDisplay();
}

void mostrarQR_MAC(String macStr) {
  QRCode qrcode; 
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  // Versión 3 = 29x29 módulos
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, macStr.c_str());

  display.clearDisplay();
  
  // Texto rotado o a la izquierda para no pisar el QR agrandado
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 10);
  display.println("ESCLAVA");
  display.setCursor(0, 25);
  display.println("MAC QR:");

  // CONFIGURACIÓN DE TAMAÑO MÁXIMO
  // Con escala = 2, el QR de 29x29 medirá 58x58 píxeles. 
  // Cabe perfecto en los 64px de alto de la pantalla.
  int escala = 2;   
  int xOffset = 65;  // Lo movemos a la derecha para dejar espacio al texto
  int yOffset = 3;   // Centrado vertical (64 - 58 = 6px libres / 2 = 3px de margen)

  // Dibujar el QR con la nueva escala
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        // Ahora dibujamos un bloque de 2x2 píxeles
        display.fillRect(xOffset + (x * escala), yOffset + (y * escala), escala, escala, SH110X_WHITE);
      }
    }
  }

  // Mostramos la MAC en texto abajo a la izquierda
  display.setCursor(0, 50);
  display.setTextSize(1);
  // Cortamos la MAC a los últimos caracteres si no entra en el ancho izquierdo, 
  // o la dejamos compacta:
  display.print(macStr.substring(9)); // Muestra solo la última parte para que no tape el QR

  display.display();
}

void mostrarQR_ConexionWiFi() {
  String cadenaWiFi = "WIFI:S:" + String(ssidMaster) + ";T:WPA;P:" + String(passMaster) + ";;";
  
  QRCode qrcode; 
  uint8_t qrcodeData[qrcode_getBufferSize(4)]; 
  // Versión 4 = 33x33 módulos
  qrcode_initText(&qrcode, qrcodeData, 4, ECC_LOW, cadenaWiFi.c_str());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  display.setCursor(0, 10);
  display.println("CONECTAR");
  display.setCursor(0, 25);
  display.println("WIFI MASTER");
  display.setCursor(0, 45);
  display.println("Escanea");

  // CONFIGURACIÓN DE TAMAÑO MÁXIMO
  // Con escala = 2, el QR de 33x33 medirá 66x66 píxeles. 
  // Como el alto es 64px, se pasaría por 2 píxeles. 
  // Para solucionarlo, forzamos la escala a 1.8 manualmente o recortamos 1px de margen,
  // pero lo más seguro para que no se deforme es usar escala = 1 y centrarlo MUY bien con un marco,
  // O usar escala = 2 sacrificando 1 píxel de borde (que el teléfono igual lee).
  // Probemos con escala = 2 y yOffset = -1 para maximizar el tamaño límite.
  
  int escala = 2;   
  int xOffset = 60; // Desplazado a la derecha
  int yOffset = -1; // Ajuste al límite del borde superior

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(xOffset + (x * escala), yOffset + (y * escala), escala, escala, SH110X_WHITE);
      }
    }
  }
  display.display();
}

void actualizarPantallaLocal(String textoRol) {

}





// [BEGIN lopaka generated]
//const char* Ip_text = "192.168.4.1";
//const char* modo_text = "MAESTRO";
//const char* Nombre_de_red_text = "Movi_WiFi";
//const char* Peso_text = "00.00";
//const char* ValorBateria_text = "78%";
void drawScreen(void) {
    display.clearDisplay();
    // Fondo blanco inferior
    display.fillRoundRect(0, 44, 128, 20, 1, 1);
    // Titulo Red
    display.setTextColor(0);
    display.setTextWrap(false);
    display.setCursor(4, 46);
    display.print("RED");
    // Nombre de red
    display.setCursor(29, 46);
    display.print(WiFi.SSID());
    // Titulo Ip
    display.setCursor(4, 55);
    display.print("IP");
    // Ip
    display.setCursor(29, 55);
    display.print(WiFi.localIP());
    // Fondo blanco superior
    display.fillRoundRect(0, 0, 128, 15, 1, 1);
    // Peso
    display.setTextColor(1);
    display.setTextSize(2);
    display.setCursor(18, 24);
    display.print(pesoActual);
    // modo
    display.setTextColor(0);
    display.setTextSize(1);
    display.setCursor(33, 4);
    if(config_esMaster)
      display.print("MAESTRO");
    else
      display.print("ESCLAVO");
    // Titulo Modo
    display.setCursor(3, 4);
    display.print("Modo:");
    // unidad Kg
    display.setTextColor(1);
    display.setTextSize(2);
    display.setCursor(87, 24);
    display.print("Kg");
    
    // ValorBateria
    uint8_t porcentaje = lecturaBateria(); // Va de 0 a 100
    display.setTextColor(0);
    display.setTextSize(1);
    display.setCursor(101, 4);
    display.printf("%d",porcentaje);display.print("%");    
    
    // Cuerpo Bateria
    display.drawRect(120, 4, 5, 7, 0);
    // Dibujo borne positivo 
    display.drawLine(121, 3, 123, 3, 0);
    // Asumimos que la variable 'porcentaje' ya fue calculada (rango 0 a 100)
    int lineas_bateria = 0;

    // Determinar cuántas líneas internas encender basándonos en el porcentaje
    if (porcentaje >= 80)      lineas_bateria = 5;
    else if (porcentaje >= 60) lineas_bateria = 4;
    else if (porcentaje >= 40) lineas_bateria = 3;
    else if (porcentaje >= 15) lineas_bateria = 2;
    else if (porcentaje > 0)   lineas_bateria = 1;
    else                       lineas_bateria = 0; // Batería completamente agotada
    // --- DIBUJO EN PANTALLA ---
    // Borne/Pitorro de la batería (Siempre encendido si quieres que se vea la silueta)
    display.drawLine(121, 3, 123, 3, 0);
    // Linea interiorBat_1 (Fondo / Carga muy baja) - Se enciende si hay al menos 1 línea
    display.drawLine(121, 9, 123, 9, (lineas_bateria >= 1) ? 0 : 1);
    // Linea interiorBat_2 - Se enciende con 2 o más líneas
    display.drawLine(121, 8, 123, 8, (lineas_bateria >= 2) ? 0 : 1);
    // Linea interiorBat_3 - Se enciende con 3 o más líneas
    display.drawLine(121, 7, 123, 7, (lineas_bateria >= 3) ? 0 : 1);
    // Linea interiorBat_4 - Se enciende con 4 o más líneas
    display.drawLine(121, 6, 123, 6, (lineas_bateria >= 4) ? 0 : 1);
    // Linea interiorBat_5 (Tope / Carga completa) - Se enciende solo si llega a 5 líneas
    display.drawLine(121, 5, 123, 5, (lineas_bateria >= 5) ? 0 : 1);



    int rssi = WiFi.RSSI();
    int barras = 0;

    if (WiFi.status() == WL_CONNECTED) {
      if (rssi >= -55)      barras = 5; 
      else if (rssi >= -65) barras = 4; 
      else if (rssi >= -75) barras = 3; 
      else if (rssi >= -85) barras = 2; 
      else                  barras = 1; 
    } else {
      barras = 0; // Desconectado (Todas las barras se borran/vuelven blancas)
    }

    // --- DIBUJO DE ANTENA EN FONDO BLANCO ---
    // (Si la barra está activa se pinta en Negro '0', si está apagada se borra con Blanco '1')

    // ant_1 (La más pequeña)
    display.drawRect(111, 50, 2, 3, (barras >= 1) ? 0 : 1);

    // ant_2
    display.drawRect(114, 49, 2, 4, (barras >= 2) ? 0 : 1);

    // ant_3
    display.drawRect(117, 48, 2, 5, (barras >= 3) ? 0 : 1);

    // ant_4
    display.drawRect(120, 47, 2, 6, (barras >= 4) ? 0 : 1);

    // ant_5 (La más alta)
    display.drawRect(123, 46, 2, 7, (barras >= 5) ? 0 : 1);
    display.display();
}
// [END lopaka generated]
#endif



