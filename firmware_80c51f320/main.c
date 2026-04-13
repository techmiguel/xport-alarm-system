#include <C8051F320.h>

// =============== DEFINICIONES ===============
typedef unsigned char u8;
typedef unsigned int u16;

// =============== CONSTANTES ===============
#define NUM_ZONES        8
#define DEBOUNCE_TIME    50     // 50ms debounce para zonas y conectividad
#define RST_BTN_HOLD_MS  3000   // 3 segundos para activar reset de IP

// =============== VARIABLES GLOBALES ===============

// --- Zonas de alarma (P1) ---
u8 zones_enabled = 0xFF;
u8 zone_stable_state[8];
u8 zone_last_raw_state[8];
u8 zone_debounce_counter[8];
u8 alarm_active = 0;

// --- Conectividad de sensores (P2) ---
// Hardware: HIGH(1) = sensor CONECTADO, LOW(0) = sensor DESCONECTADO
// Estado guardado igual al pin: 1 = conectado, 0 = desconectado
u8 zone_connected_state[8];
u8 zone_conn_last_raw[8];
u8 zone_conn_debounce[8];

// --- UART ---
u8 rx_char;
u8 rx_index = 0;
u8 rx_buffer[12];

// --- Temporizacion ---
u16 system_time_ms = 0;

// --- Boton reset IP (P0.0, activo LOW) ---
u16 rst_btn_hold_counter = 0;
u8  rst_btn_triggered = 0;

// =============== FUNCIONES BASICAS ===============

void UART0_Write(u8 dato) {
    while(!TI0);
    TI0 = 0;
    SBUF0 = dato;
}

void Send_String(u8 *str) {
    while(*str) UART0_Write(*str++);
}

void Send_Number(u8 num) {
    u8 digits[3];
    u8 i = 0;
    u8 j;

    if(num == 0) {
        UART0_Write('0');
        return;
    }
    while(num > 0) {
        digits[i++] = (num % 10) + '0';
        num /= 10;
    }
    for(j = i; j > 0; j--) {
        UART0_Write(digits[j-1]);
    }
}

// =============== INICIALIZACION ===============

void Init_System(void) {
    u8 i;
    u8 initial_zones;
    u8 initial_conn;

    // Configuracion basica
    PCA0MD &= ~0x40;       // Desactivar WDT
    OSCICN |= 0x03;        // Reloj 12MHz

    // Configuracion de puertos
    P1MDIN  = 0xFF;        // P1: entradas digitales (zonas alarma)
    P2MDIN  = 0xFF;        // P2: entradas digitales (conectividad sensores)
    P0MDIN  = 0xFF;        // P0: entradas digitales (incluye P0.0 = boton reset IP)
    P0MDOUT = 0x10;        // Solo P0.4 (TX) como push-pull

    // Crossbar
    XBR0 = 0x01;           // Habilitar UART0
    XBR1 = 0x40;           // Habilitar Crossbar

    // Configuracion UART (9600 baud @ 12MHz)
    SCON0 = 0x10;
    TMOD &= 0x0F;
    TMOD |= 0x20;          // Timer1 modo 2 (8-bit auto-reload)
    CKCON &= ~0x0B;
    CKCON |= 0x01;         // SYSCLK/4
    TH1 = 0x64;
    TR1 = 1;
    TI0 = 1;

    // Leer estado inicial
    initial_zones = P1;
    initial_conn  = P2;    // P2 HIGH(1) = sensor CONECTADO, LOW(0) = DESCONECTADO

    alarm_active = 0;
    zones_enabled = 0xFF;
    rst_btn_hold_counter = 0;
    rst_btn_triggered = 0;

    for(i = 0; i < 8; i++) {
        // Zonas de alarma
        zone_stable_state[i]   = (initial_zones >> i) & 0x01;
        zone_last_raw_state[i] = zone_stable_state[i];
        zone_debounce_counter[i] = 0;

        // Conectividad: guardar estado RAW de P2
        // 1 = desconectado (hardware HIGH), 0 = conectado (hardware LOW)
        zone_conn_last_raw[i]   = (initial_conn >> i) & 0x01;
        zone_connected_state[i] = zone_conn_last_raw[i];  // Sin inversion
        zone_conn_debounce[i]   = 0;
    }
}

// =============== DELAY 1ms ===============

void Delay_1ms(void) {
    u16 j;
    for(j = 0; j < 120; j++);
}

// =============== DETECCION DE ZONAS (P1) ===============

void Check_Zones(void) {
    u8 current_raw, zone_current, i;

    current_raw = P1;

    for(i = 0; i < 8; i++) {
        zone_current = (current_raw >> i) & 0x01;

        if(zone_current != zone_last_raw_state[i]) {
            zone_debounce_counter[i] = DEBOUNCE_TIME;
            zone_last_raw_state[i] = zone_current;
        }

        if(zone_debounce_counter[i] > 0) {
            zone_debounce_counter[i]--;

            if(zone_debounce_counter[i] == 0) {
                if(zone_current != zone_stable_state[i]) {
                    zone_stable_state[i] = zone_current;

                    // Evento: Z<zona>:<estado>
                    Send_String("Z");
                    UART0_Write(i + '0');
                    Send_String(":");
                    UART0_Write(zone_current + '0');
                    Send_String("\r\n");

                    // Intrusion: zona habilitada y abierta
                    if(zone_current == 0 && (zones_enabled & (1 << i))) {
                        alarm_active = 1;
                        Send_String("!Z");
                        UART0_Write(i + '0');
                        Send_String("\r\n");
                    }
                }
            }
        }
    }
}

// =============== DETECCION DE CONECTIVIDAD (P2) ===============
// P2 HIGH(1) = sensor CONECTADO     <- valor enviado: 1
// P2 LOW(0)  = sensor DESCONECTADO  <- valor enviado: 0
// Evento: C<zona>:<estado_raw>  (directo: 1=CONECTADO, 0=DESCONECTADO)

void Check_Connections(void) {
    u8 current_raw, raw_bit, i;

    current_raw = P2;

    for(i = 0; i < 8; i++) {
        raw_bit = (current_raw >> i) & 0x01;

        if(raw_bit != zone_conn_last_raw[i]) {
            zone_conn_debounce[i] = DEBOUNCE_TIME;
            zone_conn_last_raw[i] = raw_bit;
        }

        if(zone_conn_debounce[i] > 0) {
            zone_conn_debounce[i]--;

            if(zone_conn_debounce[i] == 0) {
                if(raw_bit != zone_connected_state[i]) {
                    zone_connected_state[i] = raw_bit;

                    // Envia el estado RAW del pin: 1=CONECTADO, 0=DESCONECTADO
                    Send_String("C");
                    UART0_Write(i + '0');
                    Send_String(":");
                    UART0_Write(raw_bit + '0');
                    Send_String("\r\n");
                }
            }
        }
    }
}

// =============== DETECCION BOTON RESET IP (P0.0) ===============
// Activo LOW (presionar = GND en P0.0)
// Mantener 3 segundos envia "IP_RST" por UART hacia el XPort/PC
// El XPort debe tener configurado el IP por defecto al que volver

void Check_RST_Button(void) {
    u8 btn = P0 & 0x01;    // Leer P0.0

    if(btn == 0) {         // Boton presionado
        rst_btn_hold_counter++;

        if(rst_btn_hold_counter >= RST_BTN_HOLD_MS && !rst_btn_triggered) {
            rst_btn_triggered = 1;
            Send_String("IP_RST\r\n");
        }
    } else {               // Boton liberado
        rst_btn_hold_counter = 0;
        rst_btn_triggered = 0;
    }
}

// =============== PROCESAMIENTO DE COMANDOS ===============

void Process_Command(u8 *cmd) {
    u8 zone;
    u8 stable_bits;
    u8 conn_bits;
    u8 i;

    // Comando STATUS: S
    // Respuesta: S <zonas_decimal> E:<habilitadas_decimal> A:<alarma> K:<conectividad_decimal>
    if(cmd[0] == 'S' && cmd[1] == 0) {
        Send_String("S ");

        stable_bits = 0;
        for(i = 0; i < 8; i++) {
            stable_bits |= (zone_stable_state[i] << i);
        }
        Send_Number(stable_bits);

        Send_String(" E:");
        Send_Number(zones_enabled);

        Send_String(" A:");
        Send_Number(alarm_active);

        // Mascara K: bit=1 sensor CONECTADO, bit=0 sensor DESCONECTADO (estado RAW de P2)
        conn_bits = 0;
        for(i = 0; i < 8; i++) {
            conn_bits |= (zone_connected_state[i] << i);
        }
        Send_String(" K:");
        Send_Number(conn_bits);

        Send_String("\r\n");
    }
    // Comando RESET: R
    else if(cmd[0] == 'R' && cmd[1] == 0) {
        alarm_active = 0;
        Send_String("R OK\r\n");
    }
    // Comando ENABLE: E <n>
    else if(cmd[0] == 'E' && cmd[1] == ' ' && cmd[2] >= '0' && cmd[2] <= '7') {
        zone = cmd[2] - '0';
        zones_enabled |= (1 << zone);
        Send_String("E Z");
        UART0_Write(zone + '0');
        Send_String("\r\n");
    }
    // Comando DISABLE: D <n>
    else if(cmd[0] == 'D' && cmd[1] == ' ' && cmd[2] >= '0' && cmd[2] <= '7') {
        zone = cmd[2] - '0';
        zones_enabled &= ~(1 << zone);
        Send_String("D Z");
        UART0_Write(zone + '0');
        Send_String("\r\n");
    }
    // Comando HELP: H
    else if(cmd[0] == 'H' && cmd[1] == 0) {
        Send_String("H S=Status R=Reset E n=Enable D n=Disable\r\n");
    }
    else {
        Send_String("?ERR\r\n");
    }
}

// =============== FUNCION PRINCIPAL ===============

void main(void) {
    Init_System();

    Send_String("\r\nALARM 8Z v3.0\r\n");
    Send_String("READY\r\n");

    while(1) {
        // 1. Verificar zonas de alarma (P1)
        Check_Zones();

        // 2. Verificar conectividad de sensores (P2)
        Check_Connections();

        // 3. Verificar boton reset IP (P0.0)
        Check_RST_Button();

        // 4. Procesar comandos UART entrantes
        if(RI0) {
            RI0 = 0;
            rx_char = SBUF0;

            if(rx_char == '\r' || rx_char == '\n' || rx_index >= 10) {
                if(rx_index > 0) {
                    rx_buffer[rx_index] = 0;
                    Process_Command(rx_buffer);
                    rx_index = 0;
                }
            } else if(rx_char >= ' ' && rx_char <= '~') {
                if(rx_index < 10) {
                    rx_buffer[rx_index++] = rx_char;
                }
            }
        }

        // 5. Delay 1ms (cadencia de muestreo y conteo de boton)
        Delay_1ms();
        system_time_ms++;
    }
}
