#ifndef COMUNICACIONES_H
#define COMUNICACIONES_H

#include <Arduino.h>
#include <WiFi.h>           // <-- DEBE IR PRIMERO QUE EL DNS
#include <WiFiMulti.h>
#include <DNSServer.h>      // <-- DEBE IR DETRÁS DE WIFI.H
#include <esp_now.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ... El resto de tu archivo Comunicaciones.h se queda exactamente igual ...
// 2. Estructura para almacenar cada red Wi-Fi en la Flash
struct RedWiFiGuardada {
    char ssid[33]; // Máx 32 caracteres + \0
    char pass[65]; // Máx 64 caracteres + \0
};

// 3. Variables globales para la gestión de Multi-WiFi
const int MAX_REDES = 4; // Permitiremos recordar hasta 4 redes Wi-Fi diferentes
extern RedWiFiGuardada listaRedes[MAX_REDES];
extern int totalRedesGuardadas;
extern WiFiMulti wifiMulti; // Instancia global del motor Multi-WiFi

// 4. Array de bytes para almacenar la MAC específica del Maestro asignado
extern uint8_t macMaestroAsignado[6];
extern bool tieneMaestroFijo; // Bandera para saber si filtra por una MAC o usa Broadcast


// 5. Estructura para que el Esclavo recuerde qué Maestros descubrió en el aire antes de vincularse
struct MaestroDescubierto {
    String mac;
    String nombre;
    unsigned long ultimaEscucha;
};

const int MAX_MAESTROS_AIRE = 4;
extern MaestroDescubierto listaMaestrosAire[MAX_MAESTROS_AIRE];
extern int totalMaestrosAire;



// Estructura para comunicación por radio ESP-NOW
typedef struct struct_message {
    int id;       // 0=Local/Master, o el ID de la rueda esclava (0 a 3)
    float peso;
    char nombre_envio[21];
} struct_message;

// Estructura interna del Maestro para recordar el estado de los esclavos vinculados
struct EsclavoVinculado {
    String mac;
    String nombre;
    float peso;
    int bateria;
    int rssi;
    unsigned long ultimaActualizacion; // Para saber si se desconectó
    String ip;
};

// Estructura auxiliar para guardar rápido (MAC de 17 caracteres + Nombre de 20)
struct EsclavoGuardado {
    char mac[18];
    char nombre[21];
};





// Variables globales de Configuración Dinámica (Cargadas de Preferences)
extern bool config_esMaster;
extern String config_tipoRed; // "router" o "ap"
extern String config_ssid;
extern String config_pass;
extern String config_nombreDisp; 

// Array dinámico para almacenar esclavos en el Maestro (Soporta hasta 4 ruedas adjuntas)
const int MAX_ESCLAVOS = 4;
extern EsclavoVinculado listaEsclavos[MAX_ESCLAVOS];
extern int totalEsclavos;
extern struct_message datosEnviados;
extern float pesoActual;


// Declaración de Funciones del Sistema
void cargarConfiguracion();
//void guardarConfiguracion(bool esMaster, String tipoRed, String ssid, String pass,String config_nombreDisp);
void inicializarRedYComunicaciones();
void inicializarServidorWeb();
void OnDataSent(const uint8_t *tx_info, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
// Funciones de envío/recepción
void setupESPNow();
void enviarPeso(float valor);
void registrarOdesvincularEsclavo(String mac, String nombre, bool vincular);
void procesarDNS();
String obtenerIPRed(); // Para que la pantalla pueda leer la IP actual
bool enviarDatosConACK(struct_message *datos);


void guardarConfiguracion(bool esMaster, String tipoRed, String config_nombreDisp);
void agregarNuevaRedWiFi(String ssid, String pass);
void eliminarRedWiFi(int index);

void asignarMaestroPreferencia(String macStr);
void registrarSobrecargaEnFlash(String nombreCelda, float kilosExceso);



#endif
