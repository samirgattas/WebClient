/*
 * Copyright (c) 2010 by WIZnet <support@wiznet.co.kr>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "w5500.h"
#include "main.h"

void SPI_transmitir8(SPI_HandleTypeDef spi, uint8_t _data);
void SPI_transmitir16(uint16_t _data);
void SPI_transmitir_buf(SPI_HandleTypeDef spi, uint8_t *_data, uint16_t _len);
uint8_t SPI_recibir8(SPI_HandleTypeDef spi);
uint16_t SPI_recibir16(SPI_HandleTypeDef spi);
void SPI_recibir_buf(SPI_HandleTypeDef spi, uint8_t *dato_recibido, uint16_t _len);

// Funcion para enviar variables de 8 bit compatible con HAL
void SPI_transmitir8(SPI_HandleTypeDef spi, uint8_t _data) {
	// Envia los 8 mas significativos
	//HAL_SPI_Transmit(&spi, &_data, 1, 100);
	SPI_transmitir_buf(spi, &_data, 1);
}

// Funcion para enviar variables de 16 bits compatible con HAL
void SPI_transmitir16(SPI_HandleTypeDef spi, uint16_t _data) {
	// Envia los 8 mas significativos
	//HAL_SPI_Transmit(&spi, (uint8_t *)((_data & 0xFF00) >> 8), 1, 100);
	SPI_transmitir8(spi, (uint8_t)((_data >> 8) & 0xFF));
	// Envia los 8 menos significativos
	//HAL_SPI_Transmit(&spi, (uint8_t *)((_data & 0x00FF)), 1, 100);
	SPI_transmitir8(spi, (uint8_t)(_data & 0xFF));

}

// Funcion para enviar buffer de 8 bits compatible con HAL
void SPI_transmitir_buf(SPI_HandleTypeDef spi, uint8_t *_data, uint16_t _len) {
	for (int i = 0; i < _len; i++) {
		HAL_SPI_Transmit(&spi, &_data[i], 1, 100);
	}

}

// Funcion para recibir variable de 8 bits compatible con HAL
uint8_t SPI_recibir8(SPI_HandleTypeDef spi){
	uint8_t dato_recibido;
	//HAL_SPI_Receive(&spi, &dato_recibido, 1, 100);
	SPI_recibir_buf(spi, &dato_recibido, 1);
	return dato_recibido;
}

// Funcion para recibir variable de 16 bits compatible con HAL
uint16_t SPI_recibir16(SPI_HandleTypeDef spi){
	uint16_t dato_recibido;
	// Recibe los 8 bits mas significativos
	//HAL_SPI_Receive(&spi, &tmp, 1, 100);
	dato_recibido = SPI_recibir8(spi) << 8;
	// Recibe los 8 bits menos significativos
	//HAL_SPI_Receive(&spi, &tmp, 1, 100);
	dato_recibido |= SPI_recibir8(spi);
	return dato_recibido;
}

// Funcion para recibir buffer de 8 bits compatible con HAL
void SPI_recibir_buf(SPI_HandleTypeDef spi, uint8_t *dato_recibido, uint16_t _len){
	for(int i = 0; i < _len; i++) {
		HAL_SPI_Receive(&spi, &dato_recibido[i], 1, 100);
	}
}


void W5500Class::init(SPI_HandleTypeDef &_spi, uint8_t _ss_pin) {
	initSS(_ss_pin);
	hspi = _spi;
	/* No haria falta esta linea porque configuramos SPI en el main
	 mSPI.beginTransaction(SPISettings(42000000));
	*/
	writeMR(0x80); // software reset the W5500 chip
	HAL_Delay(100);    // give time for reset
}

uint16_t W5500Class::getTXFreeSize(SOCKET s) {
	uint16_t val = 0, val1;
	do {
		val1 = readSnTX_FSR(s);
		if (val1 != 0)
			val = readSnTX_FSR(s);
	} while (val != val1);
	return val;
}

uint16_t W5500Class::getRXReceivedSize(SOCKET s) {
	uint16_t val = 0, val1;
	do {
		val1 = readSnRX_RSR(s);
		if (val1 != 0)
			val = readSnRX_RSR(s);
	} while (val != val1);
	return val;
}

void W5500Class::send_data_processing_offset(SOCKET s, uint16_t data_offset,
		const uint8_t *data, uint16_t len) {
	uint16_t ptr = readSnTX_WR(s);
	uint8_t cntl_byte = (0x14 + (s << 5));
	ptr += data_offset;
	write(ptr, cntl_byte, data, len);
	ptr += len;
	writeSnTX_WR(s, ptr);
}

void W5500Class::recv_data_processing(SOCKET s, uint8_t *data, uint16_t len,
		uint8_t peek) {
	uint16_t ptr = readSnRX_RD(s);
	read_data(s, ptr, data, len);
	if (!peek) {
		ptr += len;
		writeSnRX_RD(s, ptr);
	}
}

void W5500Class::write(uint16_t _addr, uint8_t _cb, uint8_t _data) {
	select_SS();
	//mSPI.write16(_addr);
	//mSPI.write16((uint16_t) (_cb << 8) | _data);
	SPI_transmitir16(hspi, _addr);
	SPI_transmitir16(hspi,(uint16_t) (_cb << 8) | _data);
	deselect_SS();
}

void W5500Class::write16(uint16_t _addr, uint8_t _cb, uint16_t _data) {
	select_SS();
	SPI_transmitir16(hspi,_addr);
	SPI_transmitir8(hspi, _cb);
	SPI_transmitir16(hspi,_data);
	deselect_SS();
}

void W5500Class::write(uint16_t _addr, uint8_t _cb, const uint8_t *_buf,
		uint16_t _len) {
	select_SS();
	SPI_transmitir16(hspi,_addr);
	SPI_transmitir8(hspi, _cb);
	SPI_transmitir_buf(hspi, (uint8_t *)_buf, _len);
	deselect_SS();
}

uint8_t W5500Class::read(uint16_t _addr, uint8_t _cb) {
	select_SS();
	SPI_transmitir16(hspi,_addr);
	SPI_transmitir8(hspi, _cb);
	//uint8_t _data = mSPI.transfer(0);
	uint8_t _data = SPI_recibir8(hspi);
	deselect_SS();
	return _data;
}

uint16_t W5500Class::read16(uint16_t _addr, uint8_t _cb) {
	select_SS();
	SPI_transmitir16(hspi,_addr);
	SPI_transmitir8(hspi, _cb);
	//uint16_t _data = mSPI.transfer16(0);
	uint16_t _data = SPI_recibir16(hspi);
	deselect_SS();
	return _data;
}

uint16_t W5500Class::read(uint16_t _addr, uint8_t _cb, uint8_t *_buf,
		uint16_t _len) {
	select_SS();
	SPI_transmitir16(hspi,_addr);
	SPI_transmitir8(hspi, _cb);
	//mSPI.read(_buf, _len);
	SPI_recibir_buf(hspi, &*_buf , _len);
	deselect_SS();
	return _len;
}

void W5500Class::execCmdSn(SOCKET s, SockCMD _cmd) {
	// Send command to socket
	writeSnCR(s, _cmd);
	// Wait for command to complete
	while (readSnCR(s))
		;
}
