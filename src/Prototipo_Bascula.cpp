
#include <Arduino.h> 
#include <Wire.h> 
#include <LittleFS.h>
#include <comunicaciones.h> 
#include <pantalla.h> 
#include <funciones.h>
#include <Preferences.h>

#include <Adafruit_NeoPixel.h>

#define PIN_LED   2   // Pin del ESP32 conectado al DI del NeoPixel
#define NUM_LEDS  1   // ¡Solo un LED!

Adafruit_NeoPixel pixel(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

extern Preferences prefs;

// En BasculaIA1.ino (Declaración global real)
HX711 scale;
float factor_calibracion = 1.0; // O el valor inicial por defecto que uses


// Variables globales de control del sensor
const int LOADCELL_DOUT_PIN = 4; 
const int LOADCELL_SCK_PIN = 5; 


float pesoActual = 0.0; 
extern float factor_calibracion; 
uint8_t bat_porcentaje;
unsigned long tiempoEntreEnviosEsclavo = 500; // Refresco base de 500ms
static unsigned long ultimoEnvioCelda = 0;
int ID_RUEDA = 0; // Se lee o computa dinámicamente si es esclavo
extern unsigned long ultimaVentaMilis;
extern float pesoUltimoChequeo;
extern bool redEnModoEco;

unsigned long ultimoBloqueoLogLoc = 0;
extern String config_nombreDisp;
extern float UMBRAL_SOBRECARGA;

void enviarPeso(float valor) {
    
        
    datosEnviados.id = bat_porcentaje; 
    datosEnviados.peso = valor;
    
    // Limpiamos el buffer de texto del paquete
    memset(datosEnviados.nombre_envio, 0, sizeof(datosEnviados.nombre_envio));
    
    // [NUEVO] Si está conectado al Router, enviamos su IP real. Si está en modo rescate, su IP de AP.
    String ipActual = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "192.168.4.1";
    
    // Copiamos la IP de forma segura al paquete de radio
    strncpy(datosEnviados.nombre_envio, ipActual.c_str(), sizeof(datosEnviados.nombre_envio) - 1);
    
    enviarDatosConACK(&datosEnviados); 

}



void setup() {
    Serial.begin(115200);
    delay(500);
    analogSetAttenuation(ADC_11db);
    pinMode(ADC_CONTROL, OUTPUT);
    pixel.begin(); // Inicializar la librería NeoPixel
    pixel.clear(); // Asegurarse de que el LED esté apagado al inicio
    pixel.setBrightness(25); // Establecer brillo al 25%
    pixel.setPixelColor(0, pixel.Color(80, 125, 150)); // Color Azul
    pixel.show(); // Envía los datos físicos al LED

    if(!LittleFS.begin(true)){
        Serial.println("Error al montar LittleFS");
    }

    #ifdef USAR_PANTALLA_OLED
    inicializarPantalla();
    #endif
    inicializarRedYComunicaciones();

    // --- LEER EL FACTOR DE CALIBRACIÓN GUARDADO ---
    prefs.begin("balanza_cfg", true); // Abrir en modo lectura
    // Si no existe el campo "facCal", usará el valor por defecto de -450.0
    factor_calibracion = prefs.getFloat("facCal", -450.0); 
    prefs.end();
    
    Serial.print("Factor de calibracion cargado desde Flash: ");
    Serial.println(factor_calibracion);

    // Inicializar Celda de Carga con el factor restaurado
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(factor_calibracion);
    scale.tare(); // Pone la balanza en cero al iniciar de forma segura

    if (config_esMaster) {
        Serial.println("Báscula lista en Modo MAESTRO.");
    } else {
        Serial.println("Báscula lista en Modo ESCLAVO.");
    }
}

void loop() {
    // 1. Mantener vivo el redireccionamiento del DNS Cautivo para los celulares
    procesarDNS(); 
    bat_porcentaje = lecturaBateria(); 
    // 2. Muestreo de la celda de carga local
    if (scale.is_ready()) { 
        pesoActual = scale.get_units(5); 

    }


    if (pesoActual > UMBRAL_SOBRECARGA && (millis() - ultimoBloqueoLogLoc > 15000)) {
        ultimoBloqueoLogLoc = millis();

        registrarSobrecargaEnFlash(config_nombreDisp + " (Local)", pesoActual);
    }


    // --- INYECTAR EN EL LOOP PRINCIPAL DEL MAESTRO ---
    if (config_esMaster) {
        // Cada 5 segundos evaluamos si el peso local varió
        static unsigned long timerChequeoEco = 0;
        if (millis() - timerChequeoEco > 5000) {
            timerChequeoEco = millis();
            
            // Si el peso varió más de 50 gramos, reiniciamos el temporizador de inactividad
            if (abs(pesoActual - pesoUltimoChequeo) > 0.05) {
                ultimaVentaMilis = millis();
                pesoUltimoChequeo = pesoActual;
                
                // Si estábamos en modo Eco y alguien tocó la balanza, despertamos a la red
                if (redEnModoEco) {
                    redEnModoEco = false;
                    Serial.println("[Modo Eco] ¡Movimiento detectado! Despertando celdas esclavas...");
                    
                    struct_message msgDespertar;
                    msgDespertar.id = -6; // Comando -6: Gestión de Energía
                    msgDespertar.peso = 1.0; // 1.0 = DESPERTAR / ALTA VELOCIDAD
                    memset(msgDespertar.nombre_envio, 0, sizeof(msgDespertar.nombre_envio));
                    
                    for(int i=0; i<totalEsclavos; i++) {
                        uint8_t mBytes[6];
                        sscanf(listaEsclavos[i].mac.c_str(), "%x:%x:%x:%x:%x:%x", &mBytes[0], &mBytes[1], &mBytes[2], &mBytes[3], &mBytes[4], &mBytes[5]);
                        esp_now_send(mBytes, (uint8_t *)&msgDespertar, sizeof(msgDespertar));
                    }
                }
            }
            
            // Si pasan 10 minutos (600,000 milisegundos) de inactividad absoluta, mandamos a dormir
            if (!redEnModoEco && (millis() - ultimaVentaMilis > 600000)) {
                redEnModoEco = true;
                Serial.println("[Modo Eco] 10 min de inactividad alcanzados. Ordenando Light Sleep a esclavos...");
                
                struct_message msgDormir;
                msgDormir.id = -6; 
                msgDormir.peso = 0.0; // 0.0 = ENTRAR EN LIGHT SLEEP
                memset(msgDormir.nombre_envio, 0, sizeof(msgDormir.nombre_envio));
                
                for(int i=0; i<totalEsclavos; i++) {
                    uint8_t mBytes[6];
                    sscanf(listaEsclavos[i].mac.c_str(), "%x:%x:%x:%x:%x:%x", &mBytes[0], &mBytes[1], &mBytes[2], &mBytes[3], &mBytes[4], &mBytes[5]);
                    esp_now_send(mBytes, (uint8_t *)&msgDormir, sizeof(msgDormir));
                }
            }
        }
    }

    
    else if (millis() - ultimoEnvioCelda > tiempoEntreEnviosEsclavo) {
        ultimoEnvioCelda = millis();
        enviarPeso(pesoActual);

    }

#ifdef USAR_PANTALLA_OLED
    drawScreen();
#endif
    // REDUCCIÓN DEL DELAY: Un delay muy alto traba el DNS. 
    // Usamos un delay pequeño o puedes quitarlo, la pantalla y el hx711 regulan su tiempo solos.
    delay(10); 
}

