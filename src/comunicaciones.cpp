#include "comunicaciones.h"
#include <esp_wifi.h>
#include <LittleFS.h>
// Instancias globales
Preferences prefs;
AsyncWebServer server(80);
DNSServer dnsServer; // <-- INSTANCIA DEL DNS CAUTIVO
struct_message datosEnviados;

// Inicialización de variables globales
bool config_esMaster = true;
String config_tipoRed = "ap";
String config_ssid = "Bascula_AP";
String config_pass = "123456789";
String config_nombreDisp = "";
// Variable global para memorizar el nombre del Maestro que nos de el ACK por radio
String nombreDelMaestroRemoto = "Buscando...";

// Variables globales para el historial de sobrecargas (Máx 3 eventos para no saturar la NVS)
String logSobrecarga1 = "Sin registros";
String logSobrecarga2 = "Sin registros";
String logSobrecarga3 = "Sin registros";

// Umbral de sobrecarga global (Por ejemplo, 45.00 kg)
float UMBRAL_SOBRECARGA = 18.00; 


volatile bool envioExitoso = false;
volatile bool callbackRecibido = false;

const byte DNS_PORT = 53; // Puerto estándar de DNS

struct EsclavoDescubierto {
    String mac;
    unsigned long ultimaEscucha;
};

RedWiFiGuardada listaRedes[MAX_REDES];
int totalRedesGuardadas = 0;
WiFiMulti wifiMulti;

EsclavoDescubierto listaDescubiertos[4];
int totalDescubiertos = 0;

EsclavoVinculado listaEsclavos[MAX_ESCLAVOS];
int totalEsclavos = 0;
extern uint8_t bat_porcentaje;

unsigned long ultimaVentaMilis = 0;
float pesoUltimoChequeo = 0.0;
bool redEnModoEco = false;


// Dirección de Broadcast por defecto para esclavos (Dirección MAC del Maestro)
uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //Asignacion de MAC Broadcast
uint8_t masterMAC[6] = {0x24, 0xDC, 0xC3, 0xAA, 0xBB, 0xCC}; // Se autocompletará en el setup

uint8_t macMaestroAsignado[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Arranca en Broadcast por defecto
bool tieneMaestroFijo = false;

MaestroDescubierto listaMaestrosAire[MAX_MAESTROS_AIRE];
int totalMaestrosAire = 0;

// Carga los ajustes desde la memoria Flash NVS al arrancar
void cargarConfiguracion() { 
    prefs.begin("balanza_cfg", true); 
    config_esMaster = prefs.getBool("esMaster", true); 
    config_tipoRed = prefs.getString("tipoRed", "ap"); 
    config_nombreDisp = prefs.getString("nombre_local", "Báscula local"); 

    String macM = prefs.getString("mac_maestro_fix", "FF:FF:FF:FF:FF:FF");
    if (macM != "FF:FF:FF:FF:FF:FF") {
        int v[6];
        if (sscanf(macM.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
            for (int i = 0; i < 6; ++i) macMaestroAsignado[i] = (uint8_t)v[i];
            tieneMaestroFijo = true;
        }
    } else {
        tieneMaestroFijo = false;
        memset(macMaestroAsignado, 0xFF, 6); // Si no hay, usa Broadcast global
    }
    
    // Cargar la lista de redes Wi-Fi múltiples
    totalRedesGuardadas = prefs.getInt("totRedes", 0);
    if(totalRedesGuardadas > MAX_REDES) totalRedesGuardadas = MAX_REDES;
    
    for (int i = 0; i < totalRedesGuardadas; i++) {
        String s = prefs.getString(("r_ssid" + String(i)).c_str(), "");
        String p = prefs.getString(("r_pass" + String(i)).c_str(), "");
        strncpy(listaRedes[i].ssid, s.c_str(), sizeof(listaRedes[i].ssid) - 1);
        strncpy(listaRedes[i].pass, p.c_str(), sizeof(listaRedes[i].pass) - 1);
        
        // Registramos la red en el motor dinámico de WiFiMulti
        wifiMulti.addAP(listaRedes[i].ssid, listaRedes[i].pass);
    }
    
    // Cargar esclavos vinculados (Tu lógica existente)
    totalEsclavos = prefs.getInt("totEsc", 0); 
    for(int i=0; i<totalEsclavos; i++) { 
        listaEsclavos[i].mac = prefs.getString(("m"+String(i)).c_str(), ""); 
        listaEsclavos[i].nombre = prefs.getString(("n"+String(i)).c_str(), "Balanza"); 
    }

        // --- INYECTAR ADENTRO DE CARGARCONFIGURACION() ANTES DEL PREFS.END() ---
    logSobrecarga1 = prefs.getString("log1", "Sin registros");
    logSobrecarga2 = prefs.getString("log2", "Sin registros");
    logSobrecarga3 = prefs.getString("log3", "Sin registros");

    prefs.end(); 
}


void guardarEsclavosEnFlash() { 
    Preferences localPrefs;
    localPrefs.begin("balanza_cfg", false); // Modo Escritura seguro
    
    // Guardamos la cantidad actual de esclavos vinculados
    localPrefs.putInt("totEsc", totalEsclavos); 
    
    // Guardamos de forma individual cada MAC y Nombre indexado
    for(int i = 0; i < totalEsclavos; i++) { 
        localPrefs.putString(("m" + String(i)).c_str(), listaEsclavos[i].mac); 
        localPrefs.putString(("n" + String(i)).c_str(), listaEsclavos[i].nombre); 
    } 
    localPrefs.end(); 
    Serial.println("-> Lista de esclavos guardada con éxito en balanza_cfg."); 
} 

void cargarEsclavosDesdeFlash() { 
    Preferences localPrefs;
    localPrefs.begin("balanza_cfg", true); // Modo lectura limpio
    
    totalEsclavos = localPrefs.getInt("totEsc", 0); 
    Serial.printf("-> [Flash NVS] Se detectaron %d esclavos registrados.\n", totalEsclavos);

    if (totalEsclavos > 0 && totalEsclavos <= 4) { 
        for (int i = 0; i < totalEsclavos; i++) { 
            listaEsclavos[i].mac = localPrefs.getString(("m" + String(i)).c_str(), ""); 
            listaEsclavos[i].nombre = localPrefs.getString(("n" + String(i)).c_str(), "Balanza"); 
            listaEsclavos[i].peso = 0.00; 
            listaEsclavos[i].bateria = 100;
            listaEsclavos[i].rssi = -99;
            listaEsclavos[i].ultimaActualizacion = millis(); 
            
            // --- REGISTRO AUTOMÁTICO EN EL HARDWARE DE ANTENA --- 
            uint8_t macBytes[6]; 
            int values[6]; 
            if (sscanf(listaEsclavos[i].mac.c_str(), "%x:%x:%x:%x:%x:%x", 
                       &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) { 
                
                for (int j = 0; j < 6; ++j) macBytes[j] = (uint8_t)values[j]; 
                
                if (!esp_now_is_peer_exist(macBytes)) { 
                    esp_now_peer_info_t peerInfo; 
                    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t)); 
                    memcpy(peerInfo.peer_addr, macBytes, 6); 
                    peerInfo.channel = WiFi.channel(); 
                    peerInfo.encrypt = false; 
                    
                    // Forzamos la interfaz correcta según el módem para que no tire 'interface is invalid'
                    peerInfo.ifidx = (WiFi.getMode() == WIFI_STA) ? WIFI_IF_STA : WIFI_IF_AP; 
                    
                    if (esp_now_add_peer(&peerInfo) == ESP_OK) { 
                        Serial.println("   [Radio] Esclavo restaurado en antena: " + listaEsclavos[i].mac); 
                    } else { 
                        Serial.println("   [Radio Error] Falló emparejamiento."); 
                    } 
                } 
            } 
        } 
        Serial.printf("-> Se restauraron de la Flash: %d esclavos oficiales.\n", totalEsclavos); 
    } 
    localPrefs.end(); 
}

void guardarConfiguracion(bool esMaster, String tipoRed, String config_nombreDisp) {
    prefs.begin("balanza_cfg", false);
    prefs.putBool("esMaster", esMaster);
    prefs.putString("tipoRed", tipoRed);
    prefs.putString("nombre_local", config_nombreDisp);
    char macMStr[18];
    snprintf(macMStr, sizeof(macMStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             macMaestroAsignado[0], macMaestroAsignado[1], macMaestroAsignado[2],
             macMaestroAsignado[3], macMaestroAsignado[4], macMaestroAsignado[5]);
    prefs.putString("mac_maestro_fix", String(macMStr));
    
    // Guardamos el bloque indexado de las redes actuales de respaldo
    prefs.putInt("totRedes", totalRedesGuardadas);
    for (int i = 0; i < totalRedesGuardadas; i++) {
        prefs.putString(("r_ssid" + String(i)).c_str(), listaRedes[i].ssid);
        prefs.putString(("r_pass" + String(i)).c_str(), listaRedes[i].pass);
    }
    prefs.end();
    Serial.println("[Flash] Identidad y lista Multi-WiFi sincronizadas.");
}


// Función callback que se ejecuta AUTOMÁTICAMENTE cuando se termina de enviar un paquete
// Función callback actualizada para el nuevo core del ESP32 (v3.x)
void OnDataSent(const uint8_t *tx_info, esp_now_send_status_t status) {
    callbackRecibido = true;
    envioExitoso = (status == ESP_NOW_SEND_SUCCESS);
}

void agregarNuevaRedWiFi(String ssid, String pass) {
    if (totalRedesGuardadas >= MAX_REDES) {
        Serial.println("Límite de redes alcanzado (Máx 4).");
        return;
    }
    // Evitar SSIDs vacíos
    ssid.trim(); 
    if (ssid == "") return;

    // Verificar si ya existe para actualizar la clave
    for (int i = 0; i < totalRedesGuardadas; i++) {
        if (String(listaRedes[i].ssid).equals(ssid)) {
            strncpy(listaRedes[i].pass, pass.c_str(), sizeof(listaRedes[i].pass) - 1);
            guardarConfiguracion(config_esMaster, config_tipoRed, config_nombreDisp);
            return;
        }
    }

    // Agregar al final del array
    strncpy(listaRedes[totalRedesGuardadas].ssid, ssid.c_str(), sizeof(listaRedes[totalRedesGuardadas].ssid) - 1);
    strncpy(listaRedes[totalRedesGuardadas].pass, pass.c_str(), sizeof(listaRedes[totalRedesGuardadas].pass) - 1);
    totalRedesGuardadas++;

    guardarConfiguracion(config_esMaster, config_tipoRed, config_nombreDisp);
}

void eliminarRedWiFi(int index) {
    if (index < 0 || index >= totalRedesGuardadas) return;

    // Desplazar el array para no dejar huecos
    for (int i = index; i < totalRedesGuardadas - 1; i++) {
        listaRedes[i] = listaRedes[i + 1];
    }
    totalRedesGuardadas--;

    guardarConfiguracion(config_esMaster, config_tipoRed, config_nombreDisp);
}

// --- CALLBACK UNIFICADO PARA ESP32 CORE v3.x (FIRMWARE ÚNICO) ---
// Usamos "const uint8_t *mac_addr" en lugar de la estructura nueva
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    struct_message packet;
    
    // En la versión antigua, la MAC viene directamente en el primer argumento
    const uint8_t *src_addr = mac_addr; 
    
    if (len == sizeof(packet)) { 
        memcpy(&packet, incomingData, sizeof(packet)); 
    } else { 
        return; 
    }

    // -------------------------------------------------------------------------
    // ROL ESCLAVO: Si esta placa actúa como Esclavo, escucha órdenes del Maestro
    // -------------------------------------------------------------------------
    if (!config_esMaster) {

        if (tieneMaestroFijo) {
            bool esMiMaestro = true;
            for(int i = 0; i < 6; i++) {
                if(src_addr[i] != macMaestroAsignado[i]) {
                    esMiMaestro = false;
                    break;
                }
            }
            if (!esMiMaestro && packet.id != -5) return; 
        }
        if (packet.id == -1) {
            extern void ejecutarTaraLocal();
            ejecutarTaraLocal();
        } 
        else if (packet.id == -2) {
            extern void ejecutarCalibracionLocal(float pesoConocido);
            ejecutarCalibracionLocal(packet.peso);
        }
        else if (packet.id == -3) {
            Serial.print("[ESP-NOW] Orden de cambio de nombre recibida: ");
            Serial.println(packet.nombre_envio);
            
            config_nombreDisp = String(packet.nombre_envio);
            guardarConfiguracion(config_esMaster, config_tipoRed, config_nombreDisp);
            Serial.println("¡Nombre local actualizado en Flash con éxito!");
        }
        else if (packet.id == -4) {
            Serial.println("[ESP-NOW] ¡Atención! Orden de REINICIO por software recibida desde el Maestro.");
            xTaskCreate([](void *param){
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart(); 
            }, "reset_esclavo_task", 2048, NULL, 1, NULL);
        }
        else if (packet.id == -5) {
            nombreDelMaestroRemoto = String(packet.nombre_envio);
            
            const uint8_t *mMAC = src_addr;
            char mStr[18];
            snprintf(mStr, sizeof(mStr), "%02X:%02X:%02X:%02X:%02X:%02X", mMAC[0], mMAC[1], mMAC[2], mMAC[3], mMAC[4], mMAC[5]);
            String macFiltroM = String(mStr);

            bool yaExisteM = false;
            for(int i=0; i<totalMaestrosAire; i++) {
                if(listaMaestrosAire[i].mac.equalsIgnoreCase(macFiltroM)) {
                    listaMaestrosAire[i].ultimaEscucha = millis();
                    listaMaestrosAire[i].nombre = String(packet.nombre_envio);
                    yaExisteM = true;
                    break;
                }
            }
            if(!yaExisteM && totalMaestrosAire < MAX_MAESTROS_AIRE) {
                listaMaestrosAire[totalMaestrosAire].mac = macFiltroM;
                listaMaestrosAire[totalMaestrosAire].nombre = String(packet.nombre_envio);
                listaMaestrosAire[totalMaestrosAire].ultimaEscucha = millis();
                totalMaestrosAire++;
            }
        }
        else if (packet.id == -6) {
            extern bool redEnModoEco; 

            if (packet.peso == 0.0) {
                Serial.println("[Energía] Orden de Light Sleep recibida.");
                redEnModoEco = true; 
                
                esp_wifi_set_ps(WIFI_PS_MAX_MODEM); 
                extern unsigned long tiempoEntreEnviosEsclavo; 
                tiempoEntreEnviosEsclavo = 10000; 
            } 
            else if (packet.peso == 1.0) {
                Serial.println("[Energía] ¡Despertar activo!");
                redEnModoEco = false; 
                
                esp_wifi_set_ps(WIFI_PS_NONE); 
                extern unsigned long tiempoEntreEnviosEsclavo;
                tiempoEntreEnviosEsclavo = 500; 
            }
        }
        return;
    }

    // -------------------------------------------------------------------------
    // ROL MAESTRO: Si esta placa actúa como Maestro, procesa reportes de peso
    // -------------------------------------------------------------------------
    if (packet.id < 0) return; 

    const uint8_t *esclavoMAC = src_addr;
    
    char macStr[18]; 
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
            esclavoMAC[0], esclavoMAC[1], esclavoMAC[2], esclavoMAC[3], esclavoMAC[4], esclavoMAC[5]);
    String macFiltro = String(macStr);

    for (int i = 0; i < totalEsclavos; i++) {
        if (listaEsclavos[i].mac.equalsIgnoreCase(macFiltro)) {
            listaEsclavos[i].peso = packet.peso;
            listaEsclavos[i].bateria = packet.id; 
            listaEsclavos[i].ultimaActualizacion = millis(); 
            
            static unsigned long ultimoBloqueoLogEsc = 0;
            if (packet.peso > UMBRAL_SOBRECARGA && (millis() - ultimoBloqueoLogEsc > 15000)) {
                ultimoBloqueoLogEsc = millis();
                registrarSobrecargaEnFlash(listaEsclavos[i].nombre, packet.peso);
            }
            
            // Corrección v2.x: En el entorno antiguo el RSSI no venía en los callbacks de ESP-NOW.
            // Lo dejamos por defecto o puedes leerlo con métodos de WiFi si es crítico, pero para compilar:
            listaEsclavos[i].rssi = -99; 
            
            listaEsclavos[i].ip = String(packet.nombre_envio);

            struct_message respuesta;
    
            if (redEnModoEco) {
                respuesta.id = -6; 
                respuesta.peso = 0.0; 
                memset(respuesta.nombre_envio, 0, sizeof(respuesta.nombre_envio));
            } else {
                respuesta.id = -5; 
                respuesta.peso = 0.0;
                memset(respuesta.nombre_envio, 0, sizeof(respuesta.nombre_envio));
                strncpy(respuesta.nombre_envio, config_nombreDisp.c_str(), sizeof(respuesta.nombre_envio) - 1);
            }

            esp_now_send(esclavoMAC, (uint8_t *)&respuesta, sizeof(respuesta));
            break; 
        }
    }

    bool yaDescubierto = false;
    for (int i = 0; i < totalDescubiertos; i++) {
        if (listaDescubiertos[i].mac.startsWith(macFiltro)) {
            listaDescubiertos[i].ultimaEscucha = millis();
            listaDescubiertos[i].mac = macFiltro + "|" + String(packet.nombre_envio);
            yaDescubierto = true;
            break;
        }
    }

    if (!yaDescubierto && totalDescubiertos < 4) {
        listaDescubiertos[totalDescubiertos].mac = macFiltro + "|" + String(packet.nombre_envio);
        listaDescubiertos[totalDescubiertos].ultimaEscucha = millis();
        totalDescubiertos++;
        Serial.println("-> [Aire] Se detectó balanza '" + String(packet.nombre_envio) + "' en la MAC: " + macFiltro);
    }
}



void inicializarRedYComunicaciones() {
    cargarConfiguracion();

    // 1. CONFIGURACIÓN DE RED UNIVERSAL (AP + STA)
    WiFi.mode(WIFI_AP_STA);
    
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.setTxPower(WIFI_POWER_11dBm); 

    // 2. INICIALIZAR MOTOR DE RADIO DE BAJO NIVEL ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("¡ERROR CRÍTICO! No se pudo inicializar ESP-NOW.");
    } else {
        Serial.println("ESP-NOW Inicializado con éxito al arranque.");
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
        esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
    }

    // 3. COMPORTAMIENTO SEGÚN EL ROL DE HARDWARE
       if (config_esMaster) {
        // --- COMPORTAMIENTO MAESTRO ---
        
        // [RECUPERADO]: Si el Maestro está configurado como AP Autónomo, genera su propio Wi-Fi fijo
        if (config_tipoRed == "ap") {
            WiFi.softAP(config_ssid.c_str(), config_pass.c_str(), 6, 0, 4);
            dnsServer.start(DNS_PORT, "*", local_IP);
            Serial.println("Maestro en Modo AP Autónomo - Canal 6 fijo. IP: 192.168.4.1");
        } 
        // Si está en modo Router (STA), activa la red de rescate e inicia el escaneo Multi-WiFi
        else {
            WiFi.softAP("Config_Maestro_Rescate", "12345678", 6, 0, 2); 
            Serial.println("Maestro en Modo Router - Buscando la mejor red Wi-Fi guardada...");
            
            int intentos = 0;
            // Solo escanea si el operador ha guardado al menos una red en la lista Multi-WiFi
            while (totalRedesGuardadas > 0 && wifiMulti.run() != WL_CONNECTED && intentos < 14) { 
                delay(500);
                Serial.print(".");
                intentos++;
            }
            
            if (WiFi.status() == WL_CONNECTED) { 
                Serial.println("\n¡Maestro Conectado exitosamente al Router!"); 
                Serial.print("IP Router: "); Serial.println(WiFi.localIP()); 
                Serial.print("Canal Router: "); Serial.println(WiFi.channel()); 
                WiFi.mode(WIFI_STA); // Apagamos el AP de rescate para limpiar el espectro
                Serial.println("AP de rescate apagado de forma segura."); 
            } else { 
                Serial.println("\n[Alerta] Maestro no conectó a ninguna red. Forzando modo AP de Rescate."); 
                WiFi.disconnect(true); 
                WiFi.mode(WIFI_AP); 
                WiFi.softAP("Config_Maestro_Rescate", "12345678", 6, 0, 2); 
            } 
        }
        
        // Las interfaces ya están estables. Cargamos los esclavos formales de la Flash
        cargarEsclavosDesdeFlash(); 
        inicializarServidorWeb(); 

    } else {
        // --- COMPORTAMIENTO ESCLAVO ---
        // El esclavo siempre opera buscando conectarse a la mejor red disponible (Multi-WiFi)
        if(config_nombreDisp == "") {
            config_ssid = "Bascula_AP_rescate";
            WiFi.softAP(config_ssid.c_str(), "12345678", 6, 0, 2); 
            Serial.println("Esclavo STA: Buscando la mejor red Wi-Fi guardada...");
        }
        else
        {
            WiFi.softAP(config_nombreDisp.c_str(), "12345678", 6, 0, 2); 
            Serial.println("Esclavo STA: Buscando la mejor red Wi-Fi guardada...");
        }
        int intentos = 0;
        while (totalRedesGuardadas > 0 && wifiMulti.run() != WL_CONNECTED && intentos < 14) { 
            delay(500);
            Serial.print(".");
            intentos++;
        }
        
        uint8_t canalDestino = 6; 
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n¡Esclavo Conectado exitosamente!");
            Serial.print("IP Router: "); Serial.println(WiFi.localIP());
            canalDestino = WiFi.channel(); 
            Serial.print("Canal Router Sincronizado: "); Serial.println(canalDestino);
            WiFi.mode(WIFI_STA); 
            Serial.println("AP de rescate del esclavo apagado de forma segura.");
        } else {
            Serial.println("\n[Alerta] Esclavo no conectó a ninguna red. Desconectando STA y fijando modo AP de Rescate.");
            WiFi.disconnect(true); 
            WiFi.mode(WIFI_AP);    
            WiFi.softAP("Config_Esclavo_Rescate", "12345678", 6, 0, 2); 
            canalDestino = 6; 
        }

        // Emparejamos el canal del Esclavo con el Maestro
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(esp_now_peer_info_t)); 
        
        // [CORREGIDO] Apuntamos la antena a la MAC del Maestro preferido (sea Broadcast o Fijo)
        memcpy(peerInfo.peer_addr, macMaestroAsignado, 6);
        
        peerInfo.channel = canalDestino; 
        peerInfo.encrypt = false;
        peerInfo.ifidx = (WiFi.getMode() == WIFI_STA) ? WIFI_IF_STA : WIFI_IF_AP;

        if (esp_now_add_peer(&peerInfo) == ESP_OK) {
            Serial.print("Enlace de Radio establecido con el Maestro en Canal "); Serial.println(canalDestino);
        }else {
            Serial.println("Error de hardware al emparejar con el Maestro.");
        }

        inicializarServidorWeb(); 
    }
}

void inicializarServidorWeb() {

    // 2. Procesador del formulario de Configuración (POST)
    // --- ENDPOINT /GUARDAR-CONFIG CORREGIDO Y SEGURO ---
    server.on("/guardar-config", HTTP_POST, [](AsyncWebServerRequest *request){
        // 1. Vinculamos de forma estricta la variable global de tu proyecto

        bool nuevoMaster = request->hasParam("modo_maestro", true); 
        String nuevoTipo = request->hasParam("tipo_red", true) ? request->getParam("tipo_red", true)->value() : "ap";
        String nuevoSsid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
        String nuevoPass = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
        
        // [CORRECCIÓN 1] Modificamos directamente la variable global, NO creamos una local
        config_nombreDisp = request->hasParam("nombre_local_disp", true) ? request->getParam("nombre_local_disp", true)->value() : "Báscula Local";
        //1.5 Agregamos la nueva red Wifi antes de guardarConfiguracion
        nuevoSsid.trim();
        if (nuevoSsid != "") {
            agregarNuevaRedWiFi(nuevoSsid, nuevoPass);
        }
        // 2. Ejecutamos tu función nativa de guardado persistente en memoria flash
        guardarConfiguracion(nuevoMaster, nuevoTipo, config_nombreDisp);
        // 3. Respondemos de inmediato al navegador para que el usuario vea el mensaje antes del reset
        String respuestaHtml = "<html><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>";
        respuestaHtml += "<h2>¡Configuración Guardada con Éxito!</h2>";
        respuestaHtml += "<p>El dispositivo se está reiniciando para aplicar los cambios...</p>";
        respuestaHtml += "</body></html>";
        request->send(200, "text/html", respuestaHtml);
        
        // [CORRECCIÓN 2] REINICIO SEGURO ASÍNCRONO
        // Eliminamos el delay() que congela la flash y usamos una tarea de fondo de Espressif
        // que esperará 2 segundos en paralelo para que el chip termine de escribir en la memoria.
        request->onDisconnect([](){
            xTaskCreate([](void *param){
                vTaskDelay(pdMS_TO_TICKS(2000));
                ESP.restart(); // El ESP32-C3 SuperMini se reinicia limpiamente
            }, "reset_task", 2048, NULL, 1, NULL);
        });
    });

    server.on("/reiniciar-local", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Reiniciando dispositivo local...");
        request->onDisconnect([](){
            xTaskCreate([](void *param){
                vTaskDelay(pdMS_TO_TICKS(1500));
                ESP.restart();
            }, "reset_local_task", 2048, NULL, 1, NULL);
        });
    });


    // --- ENDPOINT /VINCULAR-ESCLAVO ACTUALIZADO Y DEFINITIVO ---
    server.on("/vincular-esclavo", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data, len);
        
        if (error) {
            request->send(400, "text/plain", "JSON Invalido");
            return;
        }
        
        String macStr = doc["mac"].as<String>();
        String nombreAmistoso = doc["nombre"].as<String>();

        // 1. TU LÓGICA EXISTENTE PARA REGISTRAR EN LA MEMORIA DEL MAESTRO
        // Invocamos tu función nativa que guarda el esclavo en listaEsclavos y lo sella en la Flash NVS
        registrarOdesvincularEsclavo(macStr, nombreAmistoso, true); 

        // 2. PARSEAR LA MAC A BYTES BINARIOS PARA LA ANTENA
        uint8_t macBytes[6];
        int values[6];
        if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], 
                &values[3], &values[4], &values[5]) == 6) {
            
            for (int i = 0; i < 6; ++i) {
                macBytes[i] = (uint8_t)values[i];
            }
            
            // Aseguramos que el esclavo quede emparejado inmediatamente en la tabla de ESP-NOW del Maestro
            if (!esp_now_is_peer_exist(macBytes)) {
                esp_now_peer_info_t peerInfo;
                memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
                memcpy(peerInfo.peer_addr, macBytes, 6);
                peerInfo.channel = WiFi.channel();
                peerInfo.encrypt = false;
                peerInfo.ifidx = (WiFi.getMode() == WIFI_STA) ? WIFI_IF_STA : WIFI_IF_AP;
                esp_now_add_peer(&peerInfo);
            }
        }

        // 3. RESPUESTA AL NAVEGADOR
        // El Maestro confirma el almacenamiento. Acto seguido, tu JavaScript gatillará 
        // de forma automática el POST a /comando-esclavo con "renombrar" para inyectar 
        // el nombre en el hardware del esclavo usando el struct expandido.
        request->send(200, "text/plain", "Vinculacion exitosa y guardada en Flash del Maestro");
    });


    // --- ENDPOINT PARA CONTROLAR ESCLAVOS DESDE LA WEB (REESCRITO COHERENTE) ---
    server.on("/comando-esclavo", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)data, len);
        
        if (error) {
            request->send(400, "text/plain", "JSON Invalido");
            return;
        }
        String macStr = doc["mac"].as<String>();
        String comando = doc["comando"].as<String>();

        uint8_t macBytes[6];
        int values[6];
        if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], 
                &values[3], &values[4], &values[5]) == 6) {
            for (int i = 0; i < 6; ++i) macBytes[i] = (uint8_t)values[i];
        } else {
            request->send(400, "text/plain", "Formato de MAC invalido");
            return;
        }

        // Preparamos el paquete usando el struct_message universal del proyecto
        struct_message msgControl;
        
        // Por seguridad, limpiamos el buffer de texto del struct llenándolo de ceros antes de usarlo
        memset(msgControl.nombre_envio, 0, sizeof(msgControl.nombre_envio));

        if (comando == "tara") {
            msgControl.id = -1;   // Identificador de comando Tara
            msgControl.peso = 0.0;
        } else if (comando == "calibrar") {
            msgControl.id = -2;   // Identificador de comando Calibrar
            float pesoPatronEnviado = doc["valor"].as<float>(); 
            if (pesoPatronEnviado <= 0.0) pesoPatronEnviado = 1.0; 
            
            msgControl.peso = pesoPatronEnviado; 
        } 
        // --- [NUEVO] PASO B: COMANDO PARA ENVIAR EL NOMBRE POR RADIO ---
        else if (comando == "renombrar") {
            msgControl.id = -3;   // Identificador de comando Cambiar Nombre
            msgControl.peso = 0.0;
            
            // Extraemos la cadena de caracteres enviada desde el JSON de la web
            String nombreAsignado = doc["nombre"].as<String>();
            if (nombreAsignado == "") nombreAsignado = "Celda Adjunta";
            
            // Copiamos de forma segura los caracteres del String al array estático de radio (máx 20 caracteres)
            strncpy(msgControl.nombre_envio, nombreAsignado.c_str(), sizeof(msgControl.nombre_envio) - 1);
        }
        
        else if (comando == "reiniciar") {
            msgControl.id = -4;   // Código de comando: Reiniciar hardware esclavo
            msgControl.peso = 0.0;
            memset(msgControl.nombre_envio, 0, sizeof(msgControl.nombre_envio));
        }

        // Verificar emparejamiento previo de hardware
        if (!esp_now_is_peer_exist(macBytes)) {
            esp_now_peer_info_t peerInfo;
            memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
            memcpy(peerInfo.peer_addr, macBytes, 6);
            peerInfo.channel = WiFi.channel(); 
            peerInfo.encrypt = false;
            peerInfo.ifidx = (WiFi.getMode() == WIFI_STA) ? WIFI_IF_STA : WIFI_IF_AP;
            esp_now_add_peer(&peerInfo);
        }

        // Enviamos el struct_message estándar por el aire hacia el esclavo específico
        esp_err_t resultado = esp_now_send(macBytes, (uint8_t *)&msgControl, sizeof(msgControl));
        
        if (resultado == ESP_OK) {
            request->send(200, "text/plain", "Comando enviado al aire de forma exitosa");
        } else {
            request->send(500, "text/plain", "Fallo de hardware al transmitir por ESP-NOW");
        }
    });


    // --- ENDPOINT: DESVINCULAR ESCLAVO (BOTON X DE LA WEB) ---
    // Script.js envía un DELETE a /desvincular-esclavo?mac=AA:BB...
    server.on("/desvincular-esclavo", HTTP_DELETE, [](AsyncWebServerRequest *request){
        if (request->hasParam("mac")) {
            String macParaEliminar = request->getParam("mac")->value();
            macParaEliminar.toUpperCase();
            
            int indiceEncontrado = -1;
            for (int i = 0; i < totalEsclavos; i++) {
                if (listaEsclavos[i].mac.equalsIgnoreCase(macParaEliminar)) {
                    indiceEncontrado = i;
                    break;
                }
            }
            
            if (indiceEncontrado != -1) {
                // Eliminar del emparejamiento de hardware ESP-NOW
                uint8_t macBytes[6];
                int values[6];
                if (sscanf(macParaEliminar.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
                    for (int j = 0; j < 6; ++j) macBytes[j] = (uint8_t)values[j];
                    esp_now_del_peer(macBytes);
                }
                
                // Reordenar el array (desplazar elementos)
                for (int i = indiceEncontrado; i < totalEsclavos - 1; i++) {
                    listaEsclavos[i] = listaEsclavos[i + 1];
                }
                totalEsclavos--;
                
                // Actualizar la Flash con el cambio
                guardarEsclavosEnFlash();
                Serial.println("-> Esclavo eliminado desde la Web: " + macParaEliminar);
                request->send(200, "text/plain", "Eliminado");
            } else {
                request->send(404, "text/plain", "No encontrado");
            }
        } else {
            request->send(400, "text/plain", "Falta parametro mac");
        }
    });

    // 2. Comandos de acción rápidos de la web
    server.on("/tara", HTTP_POST, [](AsyncWebServerRequest *request){
        extern void ejecutarTaraLocal();
        ejecutarTaraLocal();
        request->send(200, "text/plain", "Tara Ejecutada");
    });

    server.on("/calibrar", HTTP_POST, [](AsyncWebServerRequest *request){
        extern void ejecutarCalibracionLocal(float pesoConocido);
        
        if (request->hasParam("peso_patron", true)) {
            String pesoStr = request->getParam("peso_patron", true)->value();
            float pesoConocido = pesoStr.toFloat();
            
            ejecutarCalibracionLocal(pesoConocido);
            request->send(200, "text/plain", "Calibracion Completa");
        } else {
            request->send(400, "text/plain", "Falta el parametro peso_patron");
        }
    });

    // Servir archivos estáticos desde LittleFS (HTML, CSS, JS)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.serveStatic("/assets/bootstrap.min.css", LittleFS, "/assets/bootstrap.min.css");
    server.serveStatic("/assets/style.css", LittleFS, "/assets/style.css");
    server.serveStatic("/assets/script.js", LittleFS, "/assets/script.js");
    server.serveStatic("/assets/bootstrap.bundle.min.js", LittleFS, "/assets/bootstrap.bundle.min.js");

        // --- NUEVO ENDPOINT PARA ACTUALIZACIÓN DINÁMICA ---
    // --- REEMPLAZO DEL ENDPOINT DE SINCRONIZACIÓN AL FINAL DE COMUNICACIONES.CPP ---
    server.on("/leer-datos-completos", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"peso_local\":" + String(pesoActual, 2) + ","; 
        json += "\"log1\":\"" + logSobrecarga1 + "\",";
        json += "\"log2\":\"" + logSobrecarga2 + "\",";
        json += "\"log3\":\"" + logSobrecarga3 + "\",";
        // --- DATOS DE RED Y ENERGÍA ---
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"bat_porcentaje\":" + String(bat_porcentaje) + ",";
        json += "\"mac_local\":\"" + WiFi.macAddress() + "\",";
        json += "\"modo_eco\":" + String(redEnModoEco ? "true" : "false") + ",";
            // --- [NUEVOS PARAMETROS DE INFRAESTRUCTURA] ---
        json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\","; 
        if (!config_esMaster) {
            // Si es esclavo, entrega el nombre real capturado desde la radio ESP-NOW
            json += "\"maestro_asociado\":\"" + nombreDelMaestroRemoto + "\",";
        } else {
            json += "\"maestro_asociado\":\"--\",";
        }
        // ----------------------------------------------
        json += "\"nombre_local\":\"" + config_nombreDisp + "\",";
        String ipLocalStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "192.168.4.1";
        json += "\"ip_local\":\"" + ipLocalStr + "\",";
        json += "\"es_maestro\":" + String(config_esMaster ? "true" : "false");
        
        
        // --- REVISIÓN DEL BUCLE DE ESCLAVOS DENTRO DEL ENDPOINT /LEER-DATOS-COMPLETOS ---
        if (config_esMaster) {
            json += ",\"esclavos\":[";
            for (int i = 0; i < totalEsclavos; i++) {
                json += "{";
                json += "\"mac\":\"" + listaEsclavos[i].mac + "\",";
                json += "\"nombre\":\"" + listaEsclavos[i].nombre + "\","; 
                
                // [CORRECCIÓN CRUCIAL] Aseguramos la coma después del peso
                json += "\"peso\":" + String(listaEsclavos[i].peso, 2) + ","; 
                
                // [CORRECCIÓN CRUCIAL] Aseguramos la coma después de la batería
                json += "\"bateria\":" + String(listaEsclavos[i].bateria) + ","; 
                
                // [CORRECCIÓN CRUCIAL] Aseguramos la coma después del rssi
                json += "\"rssi\":" + String(listaEsclavos[i].rssi) + ",";
                json += "\"ip\":\"" + listaEsclavos[i].ip + "\","; 
                
                json += "\"ms_desde_actualizacion\":" + String(millis() - listaEsclavos[i].ultimaActualizacion); 
                json += "}";
                if (i < totalEsclavos - 1) json += ",";
            }
            json += "],";

            // Bloque de descubiertos
            // Sección final de descubiertos en tu endpoint /leer-datos-completos:
            json += "\"descubiertos\":[";
            for (int i = 0; i < totalDescubiertos; i++) {
                // Enviamos la cadena "MAC|Nombre" que el JavaScript procesará
                json += "\"" + listaDescubiertos[i].mac + "\"";
                if (i < totalDescubiertos - 1) json += ",";
            }
            json += "]";

        }

        json += "}";
        request->send(200, "application/json", json);
    });

    // --- REEMPLAZO DEL ENDPOINT /TARA-GENERAL CORREGIDO EN EL MAESTRO ---
    server.on("/tara-general", HTTP_POST, [](AsyncWebServerRequest *request){
        // 1. Ejecutar de inmediato la Tara física en el propio Maestro local
        extern void ejecutarTaraLocal();
        ejecutarTaraLocal();

        // 2. Preparar el paquete de control universal con el ID de comando Tara (-1)
        struct_message msgControl;
        msgControl.id = -1; // -1 indica orden de Tara inalámbrica
        msgControl.peso = 0.0;

        // 3. Recorrer todos los esclavos vinculados para mandarles la orden uno por uno
        for (int i = 0; i < totalEsclavos; i++) {
            uint8_t macBytes[6];
            int values[6]; // Arreglo de enteros auxiliar obligatorio para parsear el string seguro
            
            // Convertimos el string "AA:BB:CC..." a un array binario real de 6 bytes
            if (sscanf(listaEsclavos[i].mac.c_str(), "%x:%x:%x:%x:%x:%x", 
                    &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
                
                for (int j = 0; j < 6; ++j) {
                    macBytes[j] = (uint8_t)values[j];
                }
                
                // Verificamos si por alguna razón el par se desvinculó de la tabla activa de la antena, si no, lo agregamos
                if (!esp_now_is_peer_exist(macBytes)) {
                    esp_now_peer_info_t peerInfo;
                    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
                    memcpy(peerInfo.peer_addr, macBytes, 6);
                    peerInfo.channel = WiFi.channel(); 
                    peerInfo.encrypt = false;
                    peerInfo.ifidx = (WiFi.getMode() == WIFI_STA) ? WIFI_IF_STA : WIFI_IF_AP;
                    esp_now_add_peer(&peerInfo);
                }

                // Disparamos la orden de Tara al aire directo hacia este esclavo
                esp_now_send(macBytes, (uint8_t *)&msgControl, sizeof(msgControl));
                Serial.println("[Radio] Orden de Tara General enviada a: " + listaEsclavos[i].mac);
            }
        }

        request->send(200, "text/plain", "Tara General Ejecutada");
    });

    // 1. Endpoint para entregar el JSON de redes actuales a la vista de configuración
    server.on("/leer-redes-wifi", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"redes\":[";
        for (int i = 0; i < totalRedesGuardadas; i++) {
            json += "{\"index\":" + String(i) + ",\"ssid\":\"" + String(listaRedes[i].ssid) + "\"}";
            if (i < totalRedesGuardadas - 1) json += ",";
        }
        json += "]}";
        request->send(200, "application/json", json);
    });

    // 2. Endpoint para agregar o actualizar una red desde la web
    server.on("/agregar-red-wifi", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        deserializeJson(doc, (const char*)data, len);
        String nuevoSsid = doc["ssid"].as<String>();
        String nuevoPass = doc["password"].as<String>();
        
        agregarNuevaRedWiFi(nuevoSsid, nuevoPass);
        request->send(200, "text/plain", "Red agregada");
    });

    // 3. Endpoint para eliminar una red por su índice
    server.on("/eliminar-red-wifi", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        deserializeJson(doc, (const char*)data, len);
        int idx = doc["index"].as<int>();
        
        eliminarRedWiFi(idx);
        request->send(200, "text/plain", "Red eliminada");
    });


    // 1. Endpoint para entregarle al Esclavo la lista de Maestros que lo están escuchando
    server.on("/leer-maestros-aire", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"maestros\":[";
        for (int i = 0; i < totalMaestrosAire; i++) {
            // Marcamos si este maestro de la lista es el que tiene guardado actualmente
            char targetStr[18];
            snprintf(targetStr, sizeof(targetStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    macMaestroAsignado[0], macMaestroAsignado[1], macMaestroAsignado[2],
                    macMaestroAsignado[3], macMaestroAsignado[4], macMaestroAsignado[5]);
            bool esElActual = listaMaestrosAire[i].mac.equalsIgnoreCase(String(targetStr));

            json += "{\"mac\":\"" + listaMaestrosAire[i].mac + "\",\"nombre\":\"" + listaMaestrosAire[i].nombre + "\",\"actual\":" + String(esElActual ? "true" : "false") + "}";
            if (i < totalMaestrosAire - 1) json += ",";
        }
        json += "]}";
        request->send(200, "application/json", json);
    });

    // 2. Endpoint para fijar la MAC del Maestro preferido desde el formulario
    server.on("/seleccionar-maestro", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        deserializeJson(doc, (const char*)data, len);
        String macElegida = doc["mac"].as<String>();

        int v[6];
        if (sscanf(macElegida.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
            for (int i = 0; i < 6; ++i) macMaestroAsignado[i] = (uint8_t)v[i];
            tieneMaestroFijo = (macElegida != "FF:FF:FF:FF:FF:FF");
            
            guardarConfiguracion(config_esMaster, config_tipoRed, config_nombreDisp);
            request->send(200, "text/plain", "Maestro asignado con éxito");
        } else {
            request->send(400, "text/plain", "MAC Inválida");
        }
    });



    server.begin();
}


// Función que mantiene vivo el redireccionamiento del celular
void procesarDNS() {
    if (config_esMaster && config_tipoRed == "ap") {
        dnsServer.processNextRequest();
    }
}

// Devuelve la IP correcta según el modo en el que esté operando
String obtenerIPRed() {
    if (!config_esMaster) return "Modo Esclavo";
    if (config_tipoRed == "ap") {
        return WiFi.softAPIP().toString();
    } else {
        // Si aún no se conecta al router, mostrará 0.0.0.0 hasta que obtenga IP
        return WiFi.localIP().toString();
    }
}


bool enviarDatosConACK(struct_message *datos) {
    int intentos = 0;
    const int MAX_INTENTOS = 3;
    
    // --- ACTUALIZACIÓN DINÁMICA DE CANAL ---
    // Si estamos conectados al router, aseguramos que ESP-NOW use el mismo canal actual
    if (WiFi.status() == WL_CONNECTED) {
        uint8_t canalActualRouter = WiFi.channel();
        
        // Modificamos el canal del par registrado sobre la marcha
        esp_now_peer_info_t peerInfo;
        if (esp_now_get_peer(masterMAC, &peerInfo) == ESP_OK) {
            if (peerInfo.channel != canalActualRouter) {
                peerInfo.channel = canalActualRouter;
                esp_now_mod_peer(&peerInfo);
                Serial.print("-> [Ajuste de Radio] Canal ESP-NOW actualizado a: ");
                Serial.println(canalActualRouter);
            }
        }
    }
    
    while (intentos < MAX_INTENTOS) {
        callbackRecibido = false;
        envioExitoso = false;
        
        esp_err_t resultado = esp_now_send(broadcastMAC, (uint8_t *) datos, sizeof(struct_message));
        
        if (resultado == ESP_OK) {
            int tiempoEspera = 0;
            while (!callbackRecibido && tiempoEspera < 50) {
                delay(1);
                tiempoEspera++;
            }
            if (envioExitoso) {
                //Serial.println("-> Peso enviado y RECIBIDO por el Maestro (ACK OK).");
                return true; 
            } else {
                Serial.print("-> Intento "); Serial.print(intentos + 1);
                Serial.println(" falló: El Maestro no respondió (No ACK). Reintentando...");
            }
        } else {
            Serial.println("-> Error crítico de hardware al iniciar el envío.");
        }
        intentos++;
        delay(30); // Subimos un poco el delay entre reintentos para dar respiro a la antena
    }
    Serial.println("[ALERTA] No se pudo comunicar con el Maestro tras 3 intentos.");
    return false; 
}

// --- IMPLEMENTACIÓN DE LA FUNCIÓN DE VINCULACIÓN EN EL MAESTRO (CORREGIDA Y BLINDADA) ---
void registrarOdesvincularEsclavo(String mac, String nombre, bool vincular) {
    if (vincular) {
        // Caso 1: Vincular un nuevo dispositivo
        for (int i = 0; i < totalEsclavos; i++) {
            if (listaEsclavos[i].mac.equalsIgnoreCase(mac)) {
                listaEsclavos[i].nombre = nombre; // Actualizamos el nombre si ya existía
                Serial.println("[Flash Master] Se actualizó el nombre del esclavo existente: " + mac);
                return;
            }
        }

        // Si es un esclavo nuevo y hay espacio en el arreglo (máx 4)
        if (totalEsclavos < MAX_ESCLAVOS) {
            listaEsclavos[totalEsclavos].mac = mac;
            listaEsclavos[totalEsclavos].nombre = nombre;
            listaEsclavos[totalEsclavos].peso = 0.0;
            listaEsclavos[totalEsclavos].bateria = 100;
            listaEsclavos[totalEsclavos].rssi = -99;
            listaEsclavos[totalEsclavos].ultimaActualizacion = millis();
            totalEsclavos++;
            Serial.println("[Flash Master] Esclavo registrado formalmente en memoria: " + mac + " (" + nombre + ")");
        } else {
            Serial.println("[Alerta Flash] No hay espacio disponible para más esclavos (Máximo 4).");
            return;
        }
    } else {
        // Caso 2: Desvincular / Eliminar dispositivo
        for (int i = 0; i < totalEsclavos; i++) {
            if (listaEsclavos[i].mac.equalsIgnoreCase(mac)) {
                // Desplazamos los elementos restantes del arreglo hacia la izquierda para no dejar huecos
                for (int j = i; j < totalEsclavos - 1; j++) {
                    listaEsclavos[j] = listaEsclavos[j + 1];
                }
                totalEsclavos--;
                Serial.println("[Flash Master] Esclavo eliminado de la memoria: " + mac);
                break;
            }
        }
    }

    // --- GUARDADO UNIFICADO EN LA FLASH NVS ---
    // Recuperamos las variables globales de red del Maestro
    extern bool config_esMaster;
    extern String config_tipoRed;
    extern String config_ssid;
    extern String config_pass;
    extern String config_nombreDisp; 

    // Abrimos una ÚNICA instancia de preferences para guardar TODO el bloque de configuración junto
    Preferences localPrefs;
    localPrefs.begin("balanza_cfg", false); // Abrimos en modo Escritura de forma segura

    // 1. Guardamos la configuración de red del propio equipo
    localPrefs.putBool("esMaster", config_esMaster);
    localPrefs.putString("tipoRed", config_tipoRed);
    localPrefs.putString("ssid", config_ssid);
    localPrefs.putString("pass", config_pass);
    localPrefs.putString("nombre_local", config_nombreDisp);

    // 2. Guardamos la base de datos de las celdas esclavas asociadas
    localPrefs.putInt("totEsc", totalEsclavos);
    for(int i = 0; i < totalEsclavos; i++) {
        localPrefs.putString(("m" + String(i)).c_str(), listaEsclavos[i].mac);
        localPrefs.putString(("n" + String(i)).c_str(), listaEsclavos[i].nombre);
    }

    // Cerramos el canal de forma rígida. Esto fuerza al ESP32 a sellar los bloques en la Flash
    localPrefs.end(); 
    
    Serial.println("[Flash Master] Base de datos e identidad del Maestro sincronizadas en un solo bloque NVS.");
}

void registrarSobrecargaEnFlash(String nombreCelda, float kilosExceso) {
    // Desplazamos los registros previos (el 2 pasa al 3, el 1 pasa al 2)
    logSobrecarga3 = logSobrecarga2;
    logSobrecarga2 = logSobrecarga1;
    
    // Formateamos el nuevo incidente de forma limpia
    logSobrecarga1 = "ALERTA: " + nombreCelda + " soportó " + String(kilosExceso, 2) + " kg";
    
    Serial.println("[Alerta de Seguridad] Guardando sobrecarga en NVS: " + logSobrecarga1);

    // Guardamos de forma rígida en Preferences
    Preferences localPrefs;
    localPrefs.begin("balanza_cfg", false);
    localPrefs.putString("log1", logSobrecarga1);
    localPrefs.putString("log2", logSobrecarga2);
    localPrefs.putString("log3", logSobrecarga3);
    localPrefs.end();
}

