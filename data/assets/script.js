// --- CONFIGURACIÓN DE INTERFAZ DE RED ---
// --- ACTUALIZACIÓN DE LA INTERFAZ DE CONFIGURACIÓN REACTIVA AL SWITCH ---
function actualizarInterfazConfig() {
    const switchModo = document.getElementById("switchModo");
    const sectorMaestro = document.getElementById("sector-seleccion-maestro");
    const selectorTipoRed = document.getElementById("tipo_red"); // Tu select de Modo de Red
    const sectorTipoRedContenedor = document.getElementById("sector-tipo-red"); // El div que encierra el h4 y el select (Línea resaltada en tu inspector)
    const sectorCredenciales = document.getElementById("sector-credenciales"); // El div de Multi-WiFi

    if (!switchModo) return; // Protección si no estamos en la página correcta

    // La interfaz ahora lee el estado EN VIVO del interruptor de la pantalla
    const visualmenteEsMaestro = switchModo.checked;

    if (visualmenteEsMaestro) {
        // --- COMPORTAMIENTO MAESTRO EN PANTALLA ---
        // Desocultamos inmediatamente el selector de Modo de Red (Router / AP)
        if (sectorTipoRedContenedor) sectorTipoRedContenedor.style.display = "block";
        if (sectorMaestro) sectorMaestro.style.display = "none"; // Ocultar en Maestro
        
        // Evaluamos qué opción tiene marcada el selector de red en este instante
        const tipoRedSeleccionada = selectorTipoRed ? selectorTipoRed.value : "router";
        
        if (tipoRedSeleccionada === "router") {
            // Si elige Maestro Router (STA): Muestra Multi-WiFi
            if (sectorCredenciales) sectorCredenciales.style.display = "block";
        } else {
            // Si elige Crear Red Propia (AP): Oculta Multi-WiFi porque el Maestro no se conectará a nada
            if (sectorCredenciales) sectorCredenciales.style.display = "none";
        }
    } else {
        // --- COMPORTAMIENTO ESCLAVO EN PANTALLA ---
        // Ocultamos el selector de Modo de Red (porque un esclavo siempre es esclavo)
        if (sectorTipoRedContenedor) sectorTipoRedContenedor.style.display = "none";
        if (sectorMaestro) sectorMaestro.style.display = "block"; // [NUEVO] Mostrar selector de Maestro en Esclavos
        // Mostramos obligatoriamente el administrador Multi-WiFi para que configure a qué redes se acoplará el esclavo
        if (sectorCredenciales) sectorCredenciales.style.display = "block";
    }
}



// --- VARIABLES GLOBALES DEL DASHBOARD ---
let listaEsclavos = [];
let dispositivoEsMaster = false; 
let dispositivosEnElAire = []; 


// --- INICIALIZACIÓN DEL PANEL (REVISIÓN DE VISIBILIDAD ATÓMICA) ---
function inicializarDashboard() {
    const indicadorRol = document.getElementById("indicador-rol");
    const panelMaestro = document.getElementById("panel-maestro-esclavos");
    
    // Capturamos los nuevos IDs que agregamos al HTML
    const tituloPeso = document.getElementById("titulo-peso-local");
    const infoSubtexto = document.getElementById("info-subtexto");
    
    const pesoTotalRedEl = document.getElementById("peso-total-red");
    const sectorEstadoGeneral = pesoTotalRedEl ? pesoTotalRedEl.closest('.col-lg-4') : null;

    if (dispositivoEsMaster) {
        // --- COMPORTAMIENTO MAESTRO ---
        if (indicadorRol) {
            indicadorRol.innerText = "Modo: Maestro (Concentrador)";
            indicadorRol.className = "badge badge-primary p-2";
        }
        if (tituloPeso) tituloPeso.innerText = "PESO TOTAL DE LA RED";
        if (infoSubtexto) infoSubtexto.style.display = "block";
        if (panelMaestro) panelMaestro.style.display = "block";
        if (sectorEstadoGeneral) sectorEstadoGeneral.style.display = "block"; 
        
        renderizarEsclavos();
    } else {
        // --- COMPORTAMIENTO ESCLAVO ---
        if (indicadorRol) {
            indicadorRol.innerText = "Modo: Independiente / Esclavo";
            indicadorRol.className = "badge badge-secondary p-2";
        }
        if (tituloPeso) tituloPeso.innerText = "PESO INDIVIDUAL BÁSCULA";
        if (infoSubtexto) infoSubtexto.style.display = "none"; // Oculta el contador de esclavos
        if (panelMaestro) panelMaestro.style.display = "none";
        if (sectorEstadoGeneral) sectorEstadoGeneral.style.display = "none"; 
    }
}



// --- RENDERIZADO DE TARJETAS DE ESCLAVOS ---
function renderizarEsclavos() {
    const contenedor = document.getElementById("contenedor-esclavos");
    if (!contenedor) return;
    contenedor.innerHTML = "";

    // [NUEVO] Evaluamos si la red global está en Modo Eco
    const sistemaEnEco = (window.ultimoJsonData && (window.ultimoJsonData.modo_eco === true || window.ultimoJsonData.modo_eco === "true"));
    const tiempoLimite = sistemaEnEco ? 15000 : 5000;
    
    // Estilos de la tarjeta
    
       

    if (listaEsclavos.length === 0) {
        contenedor.innerHTML = `<div class="col-12 text-center text-muted my-4">No hay básculas esclavas vinculadas todavía.</div>`;
        actualizarMetricasTotales();
        return;
    }

        listaEsclavos.forEach((esclavo, index) => {
        const estaFueraDeLinea = esclavo.antiguedadMs > tiempoLimite;
        const bordeClase = estaFueraDeLinea ? "border-danger bg-light" : (sistemaEnEco ? "border-info bg-white" : "border-primary bg-white");
        const opacidadPeso = estaFueraDeLinea ? "text-muted text-strikethrough" : "text-secondary";
            // [NUEVO] Badge de estado con icono de trébol ☘️ si está en Modo Eco
        let estadoTexto = estaFueraDeLinea ? "<span class='badge bg-danger text-white small'>OFFLINE</span>" : "<span class='badge bg-success text-white small'>ONLINE</span>";
        if (sistemaEnEco && !estaFueraDeLinea) {
            estadoTexto = "<span class='badge bg-info text-dark small'>☘️ MODO ECO</span>";
        }
        
        let batColor = "text-success";
        if (esclavo.bateria < 20) batColor = "text-danger";
        else if (esclavo.bateria < 50) batColor = "text-warning";

        contenedor.innerHTML += `
            <div class="col-md-6 col-xl-4 mb-3" id="tarjeta-esclavo-${index}">
                <div class="card p-3 border-left-3 shadow-sm ${bordeClase}" style="transition: all 0.3s ease;">
                    
                    <!-- BARRA SUPERIOR DE TELEMETRÍA (ACTUALIZADA CON ICONOS) -->
                    <div class="d-flex justify-content-between align-items-center border-bottom pb-1 mb-2 small text-muted font-weight-bold">
                        <div id="rssi-esclavo-${index}">
                            ${estadoTexto} <span class="ml-1"><i class="bi bi-broadcast"></i> ${esclavo.rssi || -99} dBm</span>
                        </div>
                        <!-- [NUEVO] Añadimos el icono bi-battery-charging o bi-battery al lado del % -->
                        <div id="bat-esclavo-${index}" class="${batColor}">
                            <i class="bi bi-battery-full mr-1"></i> ${esclavo.bateria || 0}%🔋
                        </div>
                    </div>

                    <!-- DATOS DE LA CELDA (Edición haciendo clic en el Nombre) -->
                    <div class="d-flex justify-content-between align-items-start">
                        <div>
                            <button onclick="solicitarCambioNombreEsclavo('${esclavo.mac}', '${esclavo.nombre}')" 
                                    class="btn btn-link p-0 text-dark font-weight-bold text-left align-baseline btn-edit-label" 
                                    title="Haz clic para renombrar esta celda de carga" 
                                    style="font-size: 1rem; text-decoration: none;"
                                    ${estaFueraDeLinea ? 'disabled' : ''}>
                                ${esclavo.nombre} <i class="bi bi-pencil text-muted small ml-1" style="font-size: 0.8rem;"></i>
                            </button>
                            <br>
                            <small class="text-muted font-family-monospace" style="margin-left: 6px;">MAC: ${esclavo.mac}</small>
                            <br>
                            <small class="ip-esclavo-texto font-family-monospace" style="margin-left: 6px;">
                                IP: ${estaFueraDeLinea || !esclavo.ip ? '<span class="text-muted">--</span>' : `<a href="http://${esclavo.ip}" target="_blank" class="text-info font-weight-bold" style="text-decoration: none;" title="Abrir configuración de esta celda">${esclavo.ip}</a>`}
                            </small>
                        </div>
                        <button onclick="desvincularEsclavo(${index})" class="btn btn-link text-danger btn-sm p-0" title="Eliminar">✕</button>
                    </div>
                    
                    <!-- VALOR DEL PESO -->
                    <div class="text-right my-2">
                        <span class="h3 font-weight-bold ${opacidadPeso}" id="peso-mac-${index}">
                            ${estaFueraDeLinea ? "---" : Number(esclavo.peso).toFixed(2)}
                        </span> 
                        <small class="text-muted">kg</small>
                    </div>
                    
                    <!-- BOTONERA INFERIOR COMPLETA -->
                    <div class="row pt-2 border-top">
                        <div class="col-4 px-1">
                            <button onclick="enviarComandoEsclavo('${esclavo.mac}', 'tara')" class="btn btn-outline-warning btn-sm w-100 font-weight-bold py-1" ${estaFueraDeLinea ? 'disabled' : ''}>
                                Tara
                            </button>
                        </div>
                        <div class="col-4 px-1">
                            <button onclick="prepararModalCalibrarEsclavo('${esclavo.mac}')" class="btn btn-outline-primary btn-sm w-100 font-weight-bold py-1" ${estaFueraDeLinea ? 'disabled' : ''}>
                                Calibrar
                            </button>
                        </div>
                        <div class="col-4 px-1">
                            <button onclick="reiniciarEsclavoRemoto('${esclavo.mac}', '${esclavo.nombre}')" class="btn btn-outline-danger btn-sm w-100 font-weight-bold py-1" title="Reiniciar celda por radio" ${estaFueraDeLinea ? 'disabled' : ''}>
                                <i class="bi bi-power"></i> Reset
                            </button>
                        </div>
                    </div>

                </div>
            </div>
        `;
    });

    actualizarMetricasTotales();
}

// --- ACCIÓN: REGISTRAR UN NUEVO ESCLAVO (CORREGIDO SIN JQUERY) ---
function registrarNuevoEsclavo() {
    const selectMac = document.getElementById("mac-esclavo");
    if (!selectMac) return;
    
    const mac = selectMac.value;
    const nombre = document.getElementById("nombre-esclavo").value.trim() || "Celda Adjunta";

    if (!mac) {
        alert("Por favor, selecciona una de las básculas detectadas en el menú.");
        return;
    }

    listaEsclavos.push({ mac: mac, nombre: nombre, peso: 0.00 });
    
    fetch('/vincular-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, nombre: nombre })
    })
    .then(response => {
        if(!response.ok) console.error("Error al registrar en memoria del chip.");
    });
    // Dentro de registrarNuevoEsclavo() en tu script.js:
    // Justo al lado o abajo de tu fetch('/vincular-esclavo'), agregamos este envío de control de radio:
    fetch('/comando-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, comando: "renombrar", nombre: nombre })
    })
    .then(res => {
        if(res.ok) console.log("Nombre amigable transmitido con éxito por radio hacia el Esclavo.");
    });


    // 1. Limpiamos el campo de texto del nombre para la próxima vez
    document.getElementById("nombre-esclavo").value = "";

    // 2. [SOLUCIÓN AL ERROR] Cerrar el modal usando Bootstrap 5 puro (Sin $)
    const modalElemento = document.getElementById('modalAgregarEsclavo');
    if (modalElemento) {
        // Intentamos obtener la instancia activa del modal o crear una nueva si no existe
        let modalInstancia = bootstrap.Modal.getInstance(modalElemento);
        if (!modalInstancia) {
            modalInstancia = new bootstrap.Modal(modalElemento);
        }
        modalInstancia.hide(); // Oculta el modal de forma limpia
    }

    // 3. Volvemos a dibujar las tarjetas en la pantalla principal para que aparezca la nueva
    renderizarEsclavos();
}

// --- ACCIONES INALÁMBRICAS SOBRE ESCLAVOS ---
function enviarComandoEsclavo(mac, accion) {
    console.log(`Enviando comando '${accion}' al esclavo inalámbrico: ${mac}`);

    // Modificamos el badge de estado del sistema para dar feedback inmediato al operador
    const badgeSistema = document.getElementById("sistema");
    if (badgeSistema) {
        badgeSistema.innerText = `ENVIANDO ${accion.toUpperCase()}...`;
        badgeSistema.className = "badge rounded-pill bg-warning text-dark px-3 py-2";
    }

    fetch('/comando-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, comando: accion })
    })
    .then(response => {
        if (response.ok) {
            if (badgeSistema) {
                badgeSistema.innerText = `${accion.toUpperCase()} ENVIADA`;
                badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2";
                setTimeout(() => { badgeSistema.innerText = "Conectado"; badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2"; }, 2500);
            }
        } else {
            alert(`Error al procesar el comando para la celda ${mac}`);
            if (badgeSistema) { badgeSistema.innerText = "Conectado"; badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2"; }
        }
    })
    .catch(err => {
        console.error("Error de red al enviar comando al esclavo:", err);
        if (badgeSistema) { badgeSistema.innerText = "Desconectado"; badgeSistema.className = "badge rounded-pill bg-danger text-white px-3 py-2"; }
    });
}

// --- FUNCIONES DE REINICIO REMOTO (NUEVAS) ---
function reiniciarDispositivoLocal() {
    if (!confirm("¿Estás seguro de que deseas REINICIAR esta báscula local? Se perderá la conexión por unos segundos.")) return;
    
    const badgeSistema = document.getElementById("sistema");
    if (badgeSistema) {
        badgeSistema.innerText = "REINICIANDO...";
        badgeSistema.className = "badge rounded-pill bg-danger text-white px-3 py-2";
    }

    fetch('/reiniciar-local', { method: 'POST' })
    .then(res => {
        if (res.ok) alert("Orden de reinicio enviada. La página se recargará automáticamente.");
    })
    .catch(err => console.error("Error al reiniciar local:", err));
}

function reiniciarEsclavoRemoto(mac, nombre) {
    if (!confirm(`¿Deseas enviar una orden de REINICIO de hardware a la celda '${nombre}' (${mac})?`)) return;

    console.log(`Enviando orden de reset por radio a la MAC: ${mac}`);
    
    fetch('/comando-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, comando: "reiniciar" })
    })
    .then(res => {
        if (res.ok) alert(`Orden de reinicio transmitida con éxito a la celda ${nombre}.`);
    })
    .catch(err => console.error("Error al reiniciar esclavo:", err));
}

// --- ACCIÓN: ELIMINAR UN ESCLAVO ---
function desvincularEsclavo(index) {
    if(confirm("¿Seguro que deseas eliminar este dispositivo adjunto?")) {
        const esclavo = listaEsclavos[index];
        fetch(`/desvincular-esclavo?mac=${esclavo.mac}`, { method: 'DELETE' });
        
        listaEsclavos.splice(index, 1);
        ultimasMacsDetectadasStr = "";
        renderizarEsclavos();
    }
}

// --- CÁLCULO CONSOLIDADO DE MÉTRICAS (CORREGIDO Y COMPLETO) ---
function actualizarMetricasTotales() {
    // 1. Obtener el peso actual de la Báscula Local (Maestro)
    const pesoLocalEl = document.getElementById("peso-valor");
    let pesoLocal = 0;
    
    if (pesoLocalEl) {
        pesoLocal = parseFloat(pesoLocalEl.innerText);
    }
    if (isNaN(pesoLocal)) pesoLocal = 0;

    // 2. Contar y sumar únicamente los esclavos que estén ONLINE
    let sumaEsclavos = 0;
    let esclavosOnline = 0;

    listaEsclavos.forEach(esc => {
        // Un esclavo está ONLINE si su antigüedad de datos es menor o igual a 5 segundos
        const estaOnline = esc.antiguedadMs <= 5000;
        
        if (estaOnline) {
            sumaEsclavos += parseFloat(esc.peso) || 0;
            esclavosOnline++;
        }
    });

    // 3. Actualizar el contador de "Dispositivos en Red" en la pantalla
    // Sumamos +1 para contar siempre a la báscula local del Maestro
    const metricCount = document.getElementById("total-dispositivos");
    if (metricCount) {
        metricCount.innerText = esclavosOnline + 1; 
    }

    // 4. Actualizar el "Peso Total Red" consolidado
    const pesoTotalEl = document.getElementById("peso-total-red");
    if (pesoTotalEl) {
        const pesoConsolidadoTotal = pesoLocal + sumaEsclavos;
        pesoTotalEl.innerText = `${pesoConsolidadoTotal.toFixed(2)} kg`;
    }
}

// --- ACTUALIZACIÓN EN TIEMPO REAL DESDE EL ENDPOINT ---
function iniciarMonitoreoDatos() {
    setInterval(() => {
        fetch('/leer-datos-completos')
            .then(response => response.json())
            .then(data => {
                window.ultimoJsonData = data;
                // 1. Actualizar Peso Local
                const pesoLocalEl = document.getElementById("peso-valor");
                if (pesoLocalEl) {
                    pesoLocalEl.innerText = Number(data.peso_local).toFixed(2);
                }
                // [NUEVO] Actualizar el título principal con el Nombre Amistoso de la memoria flash
                const tituloLocalEl = document.getElementById("titulo-peso-local");
                if (tituloLocalEl && data.nombre_local) {
                    tituloLocalEl.innerText = data.nombre_local; 
                }
                const macLocalEl = document.getElementById("mac-local-valor");
                if (macLocalEl && data.mac_local) {
                    macLocalEl.innerText = data.mac_local.toUpperCase(); // Forzamos mayúsculas para estética industrial
                }
                // 1. Forzar visibilidad del panel de historial únicamente si es Maestro
                const panelHistorial = document.getElementById("panel-historial-alertas");
                if (panelHistorial) {
                    panelHistorial.style.display = data.es_maestro ? "block" : "none";
                }

                // 2. Renderizar las líneas del historial en tiempo real
                const listaLogsEl = document.getElementById("lista-logs-sobrecarga");
                if (listaLogsEl && data.es_maestro) {
                    listaLogsEl.innerHTML = `
                        <li class="list-group-item py-1 border-0 ${data.log1.includes('ALERTA') ? 'text-danger font-weight-bold' : 'text-muted'}">1. ${data.log1}</li>
                        <li class="list-group-item py-1 border-0 ${data.log2.includes('ALERTA') ? 'text-danger font-weight-bold' : 'text-muted'}">2. ${data.log2}</li>
                        <li class="list-group-item py-1 border-0 ${data.log3.includes('ALERTA') ? 'text-danger font-weight-bold' : 'text-muted'}">3. ${data.log3}</li>
                    `;
                }

                // 2. Actualizar Interfaz de Red Física (Tus 4 barras HTML)
                actualizarBarrasWifi(data.rssi);
                dispositivoEsMaster = data.es_maestro; // Sincronizamos la variable global con el estado real del ESP32

                // 3. Actualizar Widget de Batería 18650
                actualizarWidgetBateria(data.bat_porcentaje);

                // 4. Cambiar indicador de modo superior e información de redes aliadas
                const modoEl = document.getElementById("modo");
                const redNombreEl = document.getElementById("red-nombre");
                const infoRedHeader = document.getElementById("info-red-header");
                const ipHeaderEl = document.getElementById("ip-header");
                // --- CORRECCIÓN EN INICIARMONITOREODATOS() EN SCRIPT.JS ---

                if (modoEl) {
                    modoEl.innerText = data.es_maestro ? "Maestro" : "Esclavo";
                    modoEl.className = data.es_maestro ? "text-primary" : "text-secondary";
                }

                if (infoRedHeader) {
                    const redActual = data.wifi_ssid || "Red Local AP";
                    const ipActual = data.ip_local || "0.0.0.0";
                    
                    if (data.es_maestro) {
                        // Si es Maestro: Muestra la red del router e inyecta la IP local al lado de forma limpia
                        infoRedHeader.innerHTML = `Router: <span class="text-dark">${redActual}</span> (<span id="ip-header" class="font-family-monospace text-primary font-weight-bold">${ipActual}</span>)`;
                    } else {
                        // Si es Esclavo: Muestra el Router (con su IP) y además el concentrador/maestro al que apunta
                        const maestroRed = data.maestro_asociado || "Desconocido";
                        infoRedHeader.innerHTML = `Router: <span class="text-dark">${redActual}</span> (<span class="font-family-monospace text-primary font-weight-bold">${ipActual}</span>) | Concentrador: <span class="text-info">${maestroRed}</span>`;
                    }
                }



                // --- [CORRECCIÓN ADAPTADA] CONTROL DE VISIBILIDAD DEL PANEL ---
                if (dispositivoEsMaster !== data.es_maestro) {
                    dispositivoEsMaster = data.es_maestro; // Sincroniza la variable global con el ESP32
                    inicializarDashboard();                // Configura los textos iniciales según el rol
                }

                // Forzamos el control atómico del Estado General y el Panel de Esclavos basados puramente en la respuesta
                const panelMaestro = document.getElementById("panel-maestro-esclavos");
                const pesoTotalRedEl = document.getElementById("peso-total-red");
                const sectorEstadoGeneral = pesoTotalRedEl ? pesoTotalRedEl.closest('.col-lg-4') : null;

                if (panelMaestro) {
                    panelMaestro.style.display = data.es_maestro ? "block" : "none";
                }
                if (sectorEstadoGeneral) {
                    sectorEstadoGeneral.style.display = data.es_maestro ? "block" : "none";
                }
                // -------------------------------------------------------------

                // 5. Si el chip es Maestro, sincronizar su arreglo dinámico de telemetría
                // --- CORRECCIÓN DEL MAPEO GLOBAL DENTRO DE INICIARMONITOREODATOS() EN SCRIPT.JS ---
                if (data.es_maestro) {
                    let necesitaRedibujar = false;

                    // Sincronizamos nuestro arreglo global mapeando TODAS las variables, incluyendo la IP
                    const nuevosEsclavos = (data.esclavos || []).map(escWeb => {
                        const estaOfflineAhora = escWeb.ms_desde_actualizacion > 5000;
                        const viejo = listaEsclavos.find(e => e.mac === escWeb.mac);
                        const estabaOfflineAntes = viejo ? (viejo.antiguedadMs > 5000) : false;
                        
                        if (estaOfflineAhora !== estabaOfflineAntes || !viejo) {
                            necesitaRedibujar = true; 
                        }

                        return {
                            mac: escWeb.mac,
                            nombre: escWeb.nombre || "Báscula Adjunta",
                            peso: escWeb.peso,
                            bateria: escWeb.bateria,
                            rssi: escWeb.rssi,
                            antiguedadMs: escWeb.ms_desde_actualizacion,
                            // [SOLUCIÓN DEFINITIVA] Mapeamos la IP que entrega tu JSON
                            ip: escWeb.ip || "" 
                        };
                    });

                    if (nuevosEsclavos.length !== listaEsclavos.length) necesitaRedibujar = true;

                    listaEsclavos = nuevosEsclavos;

                    if (necesitaRedibujar) {
                        renderizarEsclavos();
                    } 
                    else {
                        listaEsclavos.forEach((esclavo, idx) => {
                            const sistemaEnEco = (window.ultimoJsonData && (window.ultimoJsonData.modo_eco === true || window.ultimoJsonData.modo_eco === "true"));
                            const tiempoLimite = sistemaEnEco ? 15000 : 5000;
                            const estaFueraDeLinea = esclavo.antiguedadMs > tiempoLimite;

                            // 1. Refrescar Peso
                            const pesoTarjeta = document.getElementById(`peso-mac-${idx}`);
                            if (pesoTarjeta && !estaFueraDeLinea) {
                                let pesoFiltro = parseFloat(esclavo.peso) || 0;
                                if (Math.abs(pesoFiltro) <= 0.03) pesoFiltro = 0.00;
                                pesoTarjeta.innerText = pesoFiltro.toFixed(2);
                            } else if (pesoTarjeta && estaFueraDeLinea) {
                                pesoTarjeta.innerText = "---";
                            }

                            // 2. Refrescar la Señal RSSI e inyectar el Trébol ☘️ dinámico
                            const rssiContenedor = document.getElementById(`rssi-esclavo-${idx}`);
                            if (rssiContenedor) {
                                let badgeEstado = estaFueraDeLinea ? "<span class='badge bg-danger text-white small'>OFFLINE</span>" : "<span class='badge bg-success text-white small'>ONLINE</span>";
                                if (sistemaEnEco && !estaFueraDeLinea) {
                                    badgeEstado = "<span class='badge bg-info text-dark small'>☘️ MODO ECO</span>";
                                }
                                rssiContenedor.innerHTML = `${badgeEstado} <span class="ml-1"><i class="bi bi-broadcast"></i> ${esclavo.rssi || -99} dBm</span>`;
                            }
                            
                            // 3. Refrescar la Batería con el icono a la izquierda
                            const batContenedor = document.getElementById(`bat-esclavo-${idx}`);
                            if (batContenedor) {
                                let batColor = "text-success";
                                if (esclavo.bateria < 20) batColor = "text-danger";
                                else if (esclavo.bateria < 50) batColor = "text-warning";
                                batContenedor.className = batColor;
                                batContenedor.innerHTML = `<i class="bi bi-battery-full mr-1"></i> ${esclavo.bateria || 0}%🔋`;
                            }

                            // 4. Refrescar la IP en caliente
                            const tarjetaEl = document.getElementById(`tarjeta-esclavo-${idx}`);
                            if (tarjetaEl) {
                                // [OPCIONAL] Modificar color de borde en caliente si pasa a Eco
                                tarjetaEl.firstElementChild.className = `card p-3 border-left-3 shadow-sm ${estaFueraDeLinea ? 'border-danger bg-light' : (sistemaEnEco ? 'border-info bg-white' : 'border-primary bg-white')}`;

                                const ipElemento = tarjetaEl.querySelector(".ip-esclavo-texto");
                                if (ipElemento) {
                                    if (estaFueraDeLinea || !esclavo.ip || esclavo.ip === "0.0.0.0" || esclavo.ip === "--" || esclavo.ip === "") {
                                        ipElemento.innerHTML = `IP: <span class="text-muted">--</span>`;
                                    } else {
                                        ipElemento.innerHTML = `IP: <a href="http://${esclavo.ip}" target="_blank" class="text-info font-weight-bold" style="text-decoration: none;" title="Abrir configuración de esta celda">${esclavo.ip}</a>`;
                                    }
                                }
                            }
                        });
                    }

                    dispositivosEnElAire = data.descubiertos || [];
                    if (typeof actualizarSelectDescubiertos === "function") {
                        actualizarSelectDescubiertos();
                    }
                }





                // Recalcular métricas totales (Suma total de kg en red)
                actualizarMetricasTotales();

                const redOk = (data.rssi !== 0 && data.rssi < 0);
                actualizarBadgeSistema(redOk);

            })
            .catch(err => {
                console.error("Error al conectar con la báscula:", err);
                
                // Si falla la petición HTTP (batería agotada o ESP32 apagado), forzar Desconectado
                actualizarBadgeSistema(false);
                
                // Apagar también las barritas de señal en el frontend si se cae la red
                actualizarBarrasWifi(0);
                actualizarWidgetBateria(0);
            });
    }, 500);
}


// --- MANEJO DINÁMICO DE TU COMPONENTE DE SEÑAL DE 4 BARRAS ---
function actualizarBarrasWifi(rssi) {
    const textoRssi = document.getElementById("wifi-texto-rssi");
    
    // Capturamos tus 4 elementos barra del DOM
    const b1 = document.getElementById("wifi-bar-1");
    const b2 = document.getElementById("wifi-bar-2");
    const b3 = document.getElementById("wifi-bar-3");
    const b4 = document.getElementById("wifi-bar-4");

    if (!b1 || !b2 || !b3 || !b4) return;

    let barraCorte = 0;

    if (rssi !== 0 && rssi < 0) {
        if (textoRssi) textoRssi.innerText = `${rssi} dBm`;
        
        // Mapeo de niveles para tus 4 barras físicas
        if (rssi >= -60)      barraCorte = 4; // Excelente
        else if (rssi >= -72) barraCorte = 3; // Buena
        else if (rssi >= -82) barraCorte = 2; // Regular
        else                  barraCorte = 1; // Mala
    } else {
        if (textoRssi) textoRssi.innerText = "S/N";
    }

    // Encendemos o apagamos agregando/removiendo la clase 'active' que ya tiene tu CSS
    if (barraCorte >= 1) b1.classList.add("active"); else b1.classList.remove("active");
    if (barraCorte >= 2) b2.classList.add("active"); else b2.classList.remove("active");
    if (barraCorte >= 3) b3.classList.add("active"); else b3.classList.remove("active");
    if (barraCorte >= 4) b4.classList.add("active"); else b4.classList.remove("active");
}

function actualizarWidgetBateria(porcentaje) {
    const textoPorcentaje = document.getElementById("bateria-porcentaje");
    if (!textoPorcentaje) return;

    // Mostrar el número de porcentaje en texto
    textoPorcentaje.innerText = `${porcentaje}%`;

    // Capturar los 5 segmentos físicos de la batería
    const barras = [
        document.getElementById("bat-bar-1"), // Fondo (Carga baja)
        document.getElementById("bat-bar-2"),
        document.getElementById("bat-bar-3"),
        document.getElementById("bat-bar-4"),
        document.getElementById("bat-bar-5")  // Tope (Carga completa)
    ];

    // Verificar que todos los elementos existan en el DOM antes de continuar
    if (barras.some(b => !b)) return;

    let lineasActivas = 0;
    let claseColor = "bat-verde"; // Color por defecto

    // 1. Determinar número de líneas a encender
    if (porcentaje >= 80)      lineasActivas = 5;
    else if (porcentaje >= 60) lineasActivas = 4;
    else if (porcentaje >= 40) lineasActivas = 3; // Tu 42% encenderá 3 líneas
    else if (porcentaje >= 15) lineasActivas = 2;
    else if (porcentaje > 0)   lineasActivas = 1;
    else                       lineasActivas = 0;

    // 2. Determinar el color general del icono según el porcentaje
    if (porcentaje >= 50) {
        claseColor = "bat-verde";
        textoPorcentaje.className = "small font-weight-bold text-success";
    } else if (porcentaje >= 20) {
        claseColor = "bat-amarillo"; // El 42% se mostrará aquí (3 barras amarillas)
        textoPorcentaje.className = "small font-weight-bold text-warning";
    } else {
        claseColor = "bat-rojo";
        textoPorcentaje.className = "small font-weight-bold text-danger";
    }

    // 3. Pintar y colorear las barras dinámicamente
    barras.forEach((barra, index) => {
        // Limpiar clases de color previas para evitar conflictos
        barra.classList.remove("bat-verde", "bat-amarillo", "bat-rojo");

        if (index < lineasActivas) {
            barra.classList.add("active", claseColor);
        } else {
            barra.classList.remove("active");
        }
    });
}

// --- MANEJO DINÁMICO DEL ESTADO DEL SISTEMA (CONECTADO / DESCONECTADO) ---
// --- FUNCIÓN UNIFICADA DE MONITOREO DE ESTADO VISUAL ---
function actualizarBadgeSistema(estaConectadoActualmente) {
    const badgeSistema = document.getElementById("sistema");
    if (!badgeSistema) return;

    // Recuperamos el estado del JSON global que inyecta el ESP32
    // Evaluamos tanto si viene como booleano true o como string "true"
    const esModoEcoActivo = (window.ultimoJsonData && (window.ultimoJsonData.modo_eco === true || window.ultimoJsonData.modo_eco === "true"));

    if (!estaConectadoActualmente) {
        badgeSistema.innerText = "Desconectado";
        badgeSistema.className = "badge rounded-pill bg-secondary text-white px-3 py-2";
    } 
    else if (esModoEcoActivo) {
        // Si hay red pero el chip reporta inactividad, el Modo Eco toma prioridad visual absoluta
        badgeSistema.innerText = "MODO ECO (REPOSO)";
        badgeSistema.className = "badge rounded-pill bg-info text-dark font-weight-bold px-3 py-2 shadow-xs";
    } 
    else {
        badgeSistema.innerText = "Conectado";
        badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2";
    }
}





// Variable global auxiliar para memorizar la última lista de MACs vistas en el aire
let ultimasMacsDetectadasStr = "";

function actualizarSelectDescubiertos() {
    const selectMac = document.getElementById("mac-esclavo");
    if (!selectMac) return;

    // Comparación rápida del string puro para evitar parpadeos
    const macsActualesStr = dispositivosEnElAire.sort().join(",");
    if (macsActualesStr === ultimasMacsDetectadasStr) return; 
    ultimasMacsDetectadasStr = macsActualesStr;

    const valorSeleccionadoPreviamente = selectMac.value; 
    selectMac.innerHTML = '<option value="">-- Selecciona una báscula cercana --</option>';

    dispositivosEnElAire.forEach(cadenaCombinada => {
        // Separamos el string recibido usando el carácter '|' como guión de corte
        // parte[0] será la MAC pura, parte[1] será el Nombre de fábrica
        const partes = cadenaCombinada.split('|');
        const macPura = partes[0];
        const nombreFabrica = partes[1] || "Báscula Adjunta";

        // Filtramos para no listar las que el Maestro ya tiene vinculadas de forma oficial
        const yaVinculado = listaEsclavos.some(e => e.mac.toUpperCase() === macPura.toUpperCase());
        
        if (!yaVinculado) {
            const opt = document.createElement("option");
            opt.value = macPura; // El valor del selector sigue siendo la MAC limpia para tu backend
            opt.setAttribute("data-nombre-origen", nombreFabrica); // Guardamos el nombre sugerido
            opt.innerText = `${macPura} (${nombreFabrica})`; // Texto visual en el menú
            
            if (macPura === valorSeleccionadoPreviamente) {
                opt.selected = true;
            }
            selectMac.appendChild(opt);
        }
    });
}

// --- ACCIONES DE LA BÁSCULA LOCAL ---
function enviarComandoLocal(accion) {
    // accion puede ser 'tara' o 'calibrar'
    console.log(`Enviando comando local: ${accion}`);
    
    fetch(accion, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comando: accion })
    })
    .then(response => {
        if (response.ok) {
            // Cambiamos temporalmente el badge del sistema para dar feedback visual
            const badgeSistema = document.getElementById("sistema");
            if (badgeSistema) {
                badgeSistema.innerText = accion.toUpperCase() + " OK";
                badgeSistema.className = "badge rounded-pill bg-success px-3 py-2";
                
                // Restauramos el estado normal después de 2 segundos
                setTimeout(() => {
                    badgeSistema.innerText = "Estado";
                    badgeSistema.className = "badge rounded-pill bg-secondary px-3 py-2";
                }, 2000);
            }
        } else {
            console.error("Error al ejecutar comando en la báscula local.");
            alert("No se pudo ejecutar la acción en el dispositivo local.");
        }
    })
    .catch(err => console.error("Error de conexión al enviar comando:", err));
}

// --- ACCIÓN: EJECUTAR CALIBRACIÓN DESDE EL MODAL ---
function ejecutarCalibracionWeb() {
    const inputPeso = document.getElementById("peso-patron");
    if (!inputPeso) return;

    const pesoValor = parseFloat(inputPeso.value);

    // Validación básica en el navegador
    if (isNaN(pesoValor) || pesoValor <= 0) {
        alert("Por favor, ingresa un peso de calibración válido y mayor a cero.");
        return;
    }

    console.log(`Iniciando calibración local con peso patrón: ${pesoValor} kg`);

    // Usamos URLSearchParams para enviar el dato como un parámetro POST clásico (x-www-form-urlencoded)
    const datosFormulario = new URLSearchParams();
    datosFormulario.append("peso_patron", pesoValor);

    fetch('/calibrar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: datosFormulario
    })
    .then(response => {
        if (response.ok) {
            // Actualizamos el estado del sistema en la barra superior para dar feedback
            const badgeSistema = document.getElementById("sistema");
            if (badgeSistema) {
                badgeSistema.innerText = "CALIBRADO OK";
                badgeSistema.className = "badge rounded-pill bg-success px-3 py-2";
                
                setTimeout(() => {
                    badgeSistema.innerText = "Estado";
                    badgeSistema.className = "badge rounded-pill bg-secondary px-3 py-2";
                }, 3000);
            }

            // Limpiamos el campo de texto del modal
            inputPeso.value = "";

            // Ocultamos el modal automáticamente (funciona tanto para Bootstrap 4 como para 5)
            // Primero intentamos con jQuery si está cargado (Bootstrap 4)
            if (typeof $ !== 'undefined' && $('#modalCalibrarLocal').modal) {
                $('#modalCalibrarLocal').modal('hide');
            } else {
                // Si usas Bootstrap 5 nativo sin jQuery
                const modalEl = document.getElementById('modalCalibrarLocal');
                const modalInstancia = bootstrap.Modal.getInstance(modalEl);
                if (modalInstancia) modalInstancia.hide();
            }

        } else {
            response.text().then(msg => {
                alert(`Error en el chip: ${msg}`);
            });
        }
    })
    .catch(err => {
        console.error("Error de conexión al calibrar:", err);
        alert("No se pudo conectar con el ESP32 para realizar la calibración.");
    });
}

function ejecutarTaraGeneralWeb() {
    if (!confirm("¿Deseas realizar una puesta a cero (TARA) en todos los dispositivos de la red simultáneamente?")) return;

    const badgeSistema = document.getElementById("sistema");
    if (badgeSistema) {
        badgeSistema.innerText = "TARA GLOBAL...";
        badgeSistema.className = "badge rounded-pill bg-danger text-white px-3 py-2";
    }

    fetch('/tara-general', { method: 'POST' })
    .then(response => {
        if (response.ok) {
            if (badgeSistema) {
                badgeSistema.innerText = "RED EN CERO";
                badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2";
                setTimeout(() => { badgeSistema.innerText = "Conectado"; }, 2500);
            }
        }
    })
    .catch(err => console.error("Error al ejecutar Tara General:", err));
}

// --- PREPARAR EL MODAL CON LA MAC DEL ESCLAVO SELECCIONADO ---
function prepararModalCalibrarEsclavo(mac) {
    // Guardamos la MAC en el campo oculto del modal para saber a quién va dirigido el comando después
    document.getElementById("calibrar-esclavo-mac").value = mac;
    document.getElementById("peso-patron-esclavo").value = ""; // Limpiamos el valor anterior
    
    // Abrimos el modal usando Bootstrap 5 nativo (Sin jQuery)
    const modalEl = document.getElementById('modalCalibrarEsclavo');
    let modalInstancia = bootstrap.Modal.getInstance(modalEl);
    if (!modalInstancia) modalInstancia = new bootstrap.Modal(modalEl);
    modalInstancia.show();
}

// --- ACCIÓN: PROCESAR Y ENVIAR LA CALIBRACIÓN INALÁMBRICA ---
function procesarCalibracionEsclavoWeb() {
    const mac = document.getElementById("calibrar-esclavo-mac").value;
    const inputPeso = document.getElementById("peso-patron-esclavo");
    const pesoValor = parseFloat(inputPeso.value);

    if (isNaN(pesoValor) || pesoValor <= 0) {
        alert("Por favor, ingresa un peso de calibración válido mayor a cero.");
        return;
    }

    console.log(`Transmitiendo calibración remota a la celda ${mac} con peso patrón: ${pesoValor} kg`);

    const badgeSistema = document.getElementById("sistema");
    if (badgeSistema) {
        badgeSistema.innerText = "CALIBRANDO REMOTO...";
        badgeSistema.className = "badge rounded-pill bg-warning text-dark px-3 py-2";
    }

    // Enviamos un POST con formato JSON incluyendo tanto la MAC como el comando y el valor numérico
    fetch('/comando-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, comando: "calibrar", valor: pesoValor })
    })
    .then(response => {
        if (response.ok) {
            if (badgeSistema) {
                badgeSistema.innerText = "ORDEN ENVIADA OK";
                badgeSistema.className = "badge rounded-pill bg-success text-white px-3 py-2";
                setTimeout(() => { badgeSistema.innerText = "Conectado"; }, 2500);
            }
            
            // Cerrar modal automáticamente
            const modalEl = document.getElementById('modalCalibrarEsclavo');
            const modalInstancia = bootstrap.Modal.getInstance(modalEl);
            if (modalInstancia) modalInstancia.hide();
        } else {
            alert("El servidor reportó un error al procesar el comando.");
        }
    })
    .catch(err => {
        console.error("Error de red en calibración remota:", err);
    });
}

// --- ACCIÓN: CAMBIAR EL NOMBRE DE UNA CELDA DESDE SU TARJETA ---
function solicitarCambioNombreEsclavo(mac, nombreActual) {
    const nuevoNombre = prompt(`Editar nombre para la celda [${mac}]:`, nombreActual);
    
    // Si el usuario cancela o deja el espacio vacío, no hacemos nada
    if (nuevoNombre === null) return; 
    const nombreLimpio = nuevoNombre.trim();
    
    if (nombreLimpio === "") {
        alert("El nombre de la báscula no puede estar vacío.");
        return;
    }

    if (nombreLimpio.length > 20) {
        alert("El nombre es demasiado largo. Máximo 20 caracteres.");
        return;
    }

    console.log(`Renombrando celda ${mac} de '${nombreActual}' a '${nombreLimpio}'...`);

    // 1. Guardamos en la base de datos de la Flash del Maestro
    fetch('/vincular-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, nombre: nombreLimpio })
    });

    // 2. Transmitimos el cambio por radio (ESP-NOW) en tiempo real al Esclavo
    fetch('/comando-esclavo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: mac, comando: "renombrar", nombre: nombreLimpio })
    })
    .then(res => {
        if (res.ok) {
            alert(`Nombre actualizado con éxito a '${nombreLimpio}'.`);
            // Limpiamos la variable de control para forzar el refresco de las tarjetas en la pantalla de inmediato
            ultimasMacsDetectadasStr = ""; 
        }
    })
    .catch(err => console.error("Error al transmitir nuevo nombre:", err));
}

// --- SISTEMA DE GESTIÓN MULTI-WIFI INDUSTRIAL (NUEVO) ---

// Función para leer las redes desde el chip y renderizarlas en la lista
function cargarListaRedesWiFiFlash() {
    const contenedorLista = document.getElementById("lista-redes-wifi-flash");
    if (!contenedorLista) return;

    fetch('/leer-redes-wifi')
    .then(response => response.json())
    .then(data => {
        contenedorLista.innerHTML = "";

        if (!data.redes || data.redes.length === 0) {
            contenedorLista.innerHTML = `<li class="list-group-item text-center text-muted small py-3">No hay redes Wi-Fi guardadas. El equipo operará en modo AP autónomo de rescate.</li>`;
            return;
        }

        data.redes.forEach(red => {
            contenedorLista.innerHTML += `
                <li class="list-group-item d-flex justify-content-between align-items-center bg-white py-2 shadow-xs mb-1 rounded">
                    <div>
                        <i class="bi bi-wifi text-primary mr-2"></i>
                        <span class="font-weight-bold text-dark">${red.ssid}</span>
                    </div>
                    <button onclick="procesarEliminarRedWiFi(${red.index}, '${red.ssid}')" class="btn btn-link text-danger p-0 border-0" title="Eliminar esta red de la memoria">
                        <i class="bi bi-trash-fill"></i> Eliminar
                    </button>
                </li>
            `;
        });
    })
    .catch(err => {
        console.error("Error al cargar el listado Multi-WiFi:", err);
        contenedorLista.innerHTML = `<li class="list-group-item text-center text-danger small py-2">Error de conexión al leer la Flash.</li>`;
    });
}

// Acción: Enviar nueva red al microcontrolador
function procesarAgregarRedWiFi() {
    const inputSsid = document.getElementById("nuevo-ssid");
    const inputPass = document.getElementById("nuevo-password");
    
    if (!inputSsid || !inputPass) return;
    
    const ssid = inputSsid.value.trim();
    const password = inputPass.value;

    if (ssid === "") {
        alert("Por favor, ingresa un nombre de red (SSID) válido.");
        return;
    }

    if (password.length > 0 && password.length < 8) {
        alert("La contraseña Wi-Fi debe tener al menos 8 caracteres por seguridad.");
        return;
    }

    fetch('/agregar-red-wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, password: password })
    })
    .then(res => {
        if (res.ok) {
            alert(`Red Wi-Fi '${ssid}' guardada de forma segura en la Flash.`);
            inputSsid.value = "";
            inputPass.value = "";
            cargarListaRedesWiFiFlash(); // Refrescamos la lista visual de inmediato
        } else {
            alert("Límite de almacenamiento alcanzado (Máximo 4 redes).");
        }
    })
    .catch(err => console.error("Error al agregar red:", err));
}

// Acción: Solicitar baja de red por su índice
function procesarEliminarRedWiFi(index, ssid) {
    if (!confirm(`¿Estás seguro de que deseas eliminar la red '${ssid}' de la memoria de la báscula?`)) return;

    fetch('/eliminar-red-wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ index: index })
    })
    .then(res => {
        if (res.ok) {
            alert(`Red '${ssid}' removida de la Flash.`);
            cargarListaRedesWiFiFlash(); // Refrescamos la lista visual
        }
    })
    .catch(err => console.error("Error al eliminar red:", err));
}

// --- GESTIÓN DE ENLACE MULTI-MAESTRO (NUEVO) ---

// Función para leer los Maestros del aire e inyectarlos en el desplegable
function cargarListaMaestrosAire() {
    const selectMaestro = document.getElementById("selector-maestro-nvs");
    if (!selectMaestro) return;

    fetch('/leer-maestros-aire')
    .then(response => response.json())
    .then(data => {
        // Mantenemos la opción por defecto de Broadcast
        selectMaestro.innerHTML = '<option value="FF:FF:FF:FF:FF:FF">-- Broadcast Global (Cualquier Maestro) --</option>';

        if (!data.maestros || data.maestros.length === 0) {
            return; // Si no hay maestros en el aire todavía, se queda solo el broadcast
        }

        data.maestros.forEach(m => {
            const opt = document.createElement("option");
            opt.value = m.mac;
            opt.innerText = `${m.nombre} (${m.mac})`;
            if (m.actual) {
                opt.selected = true; // El script marca automáticamente el que ya tiene grabado en su Flash
            }
            selectMaestro.appendChild(opt);
        });
    })
    .catch(err => console.error("Error al leer maestros del aire:", err));
}

// Acción: Transmitir el Maestro elegido a la Flash del chip
function procesarFijarMaestroFlash() {
    const selectMaestro = document.getElementById("selector-maestro-nvs");
    if (!selectMaestro) return;

    const macElegida = selectMaestro.value;

    fetch('/seleccionar-maestro', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac: macElegida })
    })
    .then(res => {
        if (res.ok) {
            alert("Enlace de seguridad establecido. La celda solo responderá a este Maestro.");
        } else {
            alert("Error al fijar el enlace del Maestro.");
        }
    })
    .catch(err => console.error("Error de red en enlace Multi-Maestro:", err));
}


// --- INICIALIZADOR COMPARTIDO INTELIGENTE (Al final de tu script.js) ---
document.addEventListener("DOMContentLoaded", () => {
    const switchModo = document.getElementById("switchModo");
    const panelMaestro = document.getElementById("panel-maestro-esclavos");

    // 1. Encendemos el bucle de telemetría en tiempo real (Para el Header de cualquier página)
    iniciarMonitoreoDatos();
    

    // 2. Si existe el switchModo, sabemos con certeza que el usuario está en configuracion.html
    if (switchModo) {
        //console.log("Inicializando vista de Configuración...");

        switchModo.addEventListener("change", () => {
            actualizarInterfazConfig();
        });
        
        // Sincronización inicial rápida del switch de configuración con el estado real del ESP32
        fetch('/leer-datos-completos')
            .then(response => response.json())
            .then(data => {
                switchModo.checked = data.es_maestro;
                // 2. [NUEVO] Sincronizamos el campo de texto con el nombre real de la Flash
                const inputNombreLocal = document.getElementById("nombre_local_disp");
                if (inputNombreLocal && data.nombre_local) {
                    inputNombreLocal.value = data.nombre_local; 
                }
                actualizarInterfazConfig(); // Redibuja los sectores visibles (Router/AP)
                cargarListaRedesWiFiFlash();
                cargarListaMaestrosAire(); 
            })
            .catch(err => {
                console.error("Fallo al sincronizar el rol inicial:", err);
                actualizarInterfazConfig();
                //cargarListaRedesWiFiFlash(); 
            });
            
    } 
    // 3. Si existe el panelMaestro, sabemos que el usuario está en el index.html principal
    else if (panelMaestro) {
        //console.log("Inicializando vista de Dashboard Principal...");
        inicializarDashboard();
    }
});

document.addEventListener("change", (e) => {
    if (e.target && e.target.id === "mac-esclavo") {
        const select = e.target;
        const opcionSeleccionada = select.options[select.selectedIndex];
        const inputNombre = document.getElementById("nombre-esclavo");
        
        if (opcionSeleccionada && inputNombre) {
            // Jalamos el atributo que separamos con el split
            const nombreOrigen = opcionSeleccionada.getAttribute("data-nombre-origen");
            inputNombre.value = nombreOrigen || ""; 
        }
    }
});
