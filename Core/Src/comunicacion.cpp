/*
 * comunicacion.cpp
 *
 *  Created on: Sep 4, 2020
 *      Author: xx
 */

// ############################################################################
// INCLUDE
#include "comunicacion.h"

// ############################################################################
// DEFINE

// Declarar define SERVER en comunicacion.h si se quiere configurar como cliente
#ifdef SERVER
#define MAC				0xE4, 0xAB, 0x89, 0x1B, 0xF1, 0xA1
#define IP_PROPIA 		192, 168, 88, 30
#define IP_SERVIDOR		192, 168, 88, 70
#else
#define MAC				0xE4, 0xAB, 0x89, 0x1B, 0xF1, 0xA0
#define IP_PROPIA 		192, 168, 88, 40
#define IP_SERVIDOR		192, 168, 88, 30
#endif

#define PUERTO			3030

#define MAX_BUFFER	20

#define INICIALIZADOR	':'
#define FINALIZADOR	';'

// ############################################################################
// VARIABLES PRIVADAS

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;

// Estado y subestado de la maquina de estados de la comunicacion
uint16_t estado = 0;
uint16_t sub_estado = 1;

// Mac del dispositivo
uint8_t mac[6] = { MAC };

//IP del servidor al que se conectara el cliente
IPAddress ip_server(IP_SERVIDOR);

IPAddress ip(IP_PROPIA);
IPAddress myDns(192, 168, 0, 1);

// Objeto cliente
EthernetClient client;

// Objeto servidor
EthernetServer server(PUERTO);

// Buffer de lectura para comunicacion
static uint8_t buffer_rx[MAX_BUFFER];

// Buffer de transmision para comunicacion
static uint8_t buffer_tx[MAX_BUFFER];

// Indica que debe responder/Indica que esta esperando respuesta
bool mustResponder;

// Indica que debe enviar consulta
bool isConsulta = true;

// Time out para comunicacion. 1 = 1ms
int16_t tout_comunicacion = 0;

// Flag auxiliar para prueba
bool flag = true;

// ############################################################################
// CABECERA DE FUNCIONES PRIVADAS
bool debe_responder(void);
bool no_debe_responder(void);
void print_consulta(void);
void set_tout(int t);
void limpiar_buffer(uint8_t _bu[]);
bool buffer_empty(uint8_t _bu[]);

// ############################################################################
// CABECERA DE FUNCIONES INTERNAS
bool debe_responder() {
	mustResponder = true;
	return mustResponder;
}

bool no_debe_responder() {
	mustResponder = false;
	return mustResponder;
}

// Pregunta el estado de la torreta
void preguntar_consulta() {
	char _buf[3];
	_buf[0] = INICIALIZADOR;
	_buf[1] = 'C';
	_buf[2] = FINALIZADOR;
	client.print(_buf);
}

// Setea el tout
void set_tout(int t) {
	tout_comunicacion = t;
}

// Limpia el contenido del buffer
void limpiar_buffer(uint8_t _buf[]) {
	memset(_buf, '\0', MAX_BUFFER);
}

// Devuelve True en caso de que el primer caracter de _buf sea nulo
bool buffer_empty(uint8_t _buf[]) {
	return _buf[0] == '\0';
}
// ############################################################################
// FUNCIONES PUBLICAS

// ---------- Cliente -------------

void inicializar_cliente(SPI_HandleTypeDef &_spi, UART_HandleTypeDef &_uart) {
	HAL_InitTick(0);	// Inicia interrupcion de SysTick cada 1ms para millis()
	Ethernet.init(hspi1, 1);
	HAL_UART_Transmit(&huart1, (uint8_t*) "Inicializa Ethernet con DHCP\n", 29,
			100);
	if (!Ethernet.begin(mac)) {
		HAL_UART_Transmit(&huart1, (uint8_t*) "Fallo usando DHCP\n", 18, 100);
		Ethernet.begin(mac, ip, myDns);
	} else {
		HAL_UART_Transmit(&huart1, (uint8_t*) "Conecto usando DHCP\n", 20, 100);
	}
}

void me_cliente(SPI_HandleTypeDef &_spi, UART_HandleTypeDef &_uart) {
	static uint8_t idx_buffer;
	int16_t length;

	switch (estado) {
	case 0: // Obtiene direccion IP
		inicializar_cliente(_spi, _uart);
		estado++;
		break;
	case 1:	// Conecta con el servidor
		HAL_UART_Transmit(&huart1, (uint8_t*) "*****\n", 6, 100);
		//HAL_Delay(1000);
		HAL_UART_Transmit(&huart1, (uint8_t*) "Conectando\n", 11, 100);
		if (client.connect(ip_server, PUERTO)) {
			// Entra si conecto con el servidor
			HAL_UART_Transmit(&huart1, (uint8_t*) "Conectado\n", 10, 100);
			// -- BORRAR --
			buffer_tx[0] = ':';
			buffer_tx[1] = 'C';
			buffer_tx[2] = ';';
			// ------------
			estado++;
		}
		break;
	case 2: // Envia msj al servidor
		if (!buffer_empty(buffer_tx)) {
			// Entra si hay msj para enviar

			if (buffer_tx[1] == 'C') {
				debe_responder();
			} else {
				length = 1;
				no_debe_responder();
			}
			client.print((char *)buffer_tx);
			if (mustResponder) {
				// Entra si espera una respuesta
				limpiar_buffer(buffer_rx);	// Limpia buffer de recepcion
				sub_estado = 1;
				estado++;
				set_tout(10);	// tout de 10 ms
			} else {
				//limpiar_buffer(buffer_tx);	// Limpia buffer de transmision
				// -- BORRAR --
				buffer_tx[0] = ':';
				buffer_tx[1] = 'C';
				buffer_tx[2] = ';';
				// ------------
				estado = 2;
			}
		}
		// Aca hay que ver si podemos saber si nos desconectamos del server
		break;
	case 3: // Espera respuesta del servidor
		switch (sub_estado) {
		case 1:	// Lee la respuesta
			length = client.available();
			if (length > 0) {
				// Entra si recibio algo
				if (length > MAX_BUFFER) {
					// Entra si recibe mas caracteres de los que puede
					// almacenar el buffer
					length = MAX_BUFFER;
				}
				client.read(buffer_rx, length);
				idx_buffer = 0;
				sub_estado++;
			} else if (tout_comunicacion == 0) {
				// Entra si se cumple el tout
				estado = 5;	// Detiene el cliente
			}
			break;
		case 2:	// Busca los terminadores
			if (buffer_rx[0] == INICIALIZADOR) {
				// Entra si recibio el inicializador trama
				for (int i = 0; i < MAX_BUFFER; i++) {
					if (buffer_rx[i] == FINALIZADOR) {
						// Entra si recibio el finalizar de trama
						idx_buffer = i;
						break;
					}
				}
			}
			sub_estado++;
			break;
		case 3:	// Verifica que esten ambos
			if (buffer_rx[0] == INICIALIZADOR
					&& buffer_rx[idx_buffer] == FINALIZADOR) {
				// Entra si contiene el iniciador y finalizador
				estado++;
			} else {
				estado = 2;	// Reenvia el msj
			}
			break;
		}
		break;
	case 4: // Interpreta msj recibido
		if (buffer_rx[1] == '1') {
			// -- BORRAR ---
			buffer_tx[0] = ':';
			buffer_tx[1] = '1';
			buffer_tx[2] = ':';
			// -------------
		} else if (buffer_rx[1] == '0') {
			// -- BORRAR ---
			buffer_tx[0] = ':';
			buffer_tx[1] = '0';
			buffer_tx[2] = ':';
			// -------------
		}
		if (client.connected()) {
			// Entra si el cliente sigue conectado al servidor
			estado = 2;
		} else {
			estado++;
		}
		break;
	case 5: // Desconecta del servidor
		limpiar_buffer(buffer_tx);	// Resetea buffer_tx
		limpiar_buffer(buffer_rx);	// Resetea buffer_rx

		client.stop();	// Detiene el cliente
		estado = 1;
		break;
	}
}

//---------- Servidor --------------

void inicializar_servidor(SPI_HandleTypeDef &_spi, UART_HandleTypeDef &_uart) {
	HAL_InitTick(0);
	Ethernet.init(hspi1, 1);
	HAL_UART_Transmit(&huart1, (uint8_t*) "Inicializa Ethernet con IP fija\n",
			32, 100);
	Ethernet.begin(mac, ip);
	server.begin();
	HAL_UART_Transmit(&huart1, (uint8_t*) "Hecho\n", 6, 100);
}

// ############################################################################
