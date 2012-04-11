/* Hey EMACS -*- linux-c -*- */
/* $Id: cmd73.c 1327 2005-07-05 15:42:00Z roms $ */

/*  libticalcs - Ti Calculator library, a part of the TiLP project
 *  Copyright (C) 1999-2005  Romain Li�vin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
  This unit handles TI73 & TI83+ & TI84+ commands.
*/

#include <string.h>
#include <stdio.h>

#include <ticonv.h>
#include "ticalcs.h"
#include "dbus_pkt.h"
#include "error.h"
#include "logging.h"
#include "macros.h"

#ifdef _MSC_VER
#pragma warning( disable : 4761 )
#endif

// Shares some commands between TI73 & 83+ & 84+
#define PC_TI7383 ((handle->model == CALC_TI73) ? PC_TI73 : PC_TI83p)
#define TI7383_PC ((handle->model == CALC_TI73) ? TI73_PC : TI83p_PC)
#define TI7383_BKUP ((handle->model == CALC_TI73) ? TI73_BKUP : TI83p_BKUP)
#define EXTRAS ((handle->model == CALC_TI83P) || (handle->model == CALC_TI84P) ? 2 : 0)

/* Variable (std var header: NUL padded, fixed length) */
int ti73_send_VAR_h(CalcHandle* handle, uint16_t varsize, uint8_t vartype, const char *varname, uint8_t varattr)
{
  uint8_t buffer[16];

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;
  memcpy(buffer + 3, varname, 8);
  buffer[11] = 0x00;
  buffer[12] = (varattr == ATTRB_ARCHIVED) ? 0x80 : 0x00;

  printf(" PC->TI: VAR (size=0x%04X, id=%02X, name=%s, attr=%i)",
		varsize, vartype, varname, varattr);

  if (vartype != TI7383_BKUP) 
  {	// backup: special header
    pad_buffer(buffer + 3, '\0');
    TRYF(dbus_send(handle, PC_TI7383, CMD_VAR, 11 + EXTRAS, buffer));
  } 
  else 
  {
    TRYF(dbus_send(handle, PC_TI7383, CMD_VAR, 9, buffer));
  }

  return 0;
}

/* FLASH (special var header: size, id, flag, offset, page) */
int ti73_send_VAR2_h(CalcHandle* handle, uint32_t length, uint8_t type, uint8_t flag,
		   uint16_t offset, uint16_t page)
{
  uint8_t buffer[11];

  buffer[0] = LSB(LSW(length));
  buffer[1] = MSB(LSW(length));
  buffer[2] = type;
  buffer[3] = LSB(MSW(length));
  buffer[4] = MSB(MSW(length));
  buffer[5] = flag;
  buffer[6] = LSB(offset);
  buffer[7] = MSB(offset);
  buffer[8] = LSB(page);
  buffer[9] = MSB(page);

  printf(" PC->TI: VAR (size=0x%04X, id=%02X, flag=%02X, offset=%04X, page=%02X)",
       length, type, flag, offset, page);

  TRYF(dbus_send(handle, PC_TI7383, CMD_VAR, 10, buffer));

  return 0;
}

int ti73_send_CTS_h(CalcHandle* handle)
{
  printf(" PC->TI: CTS");
  TRYF(dbus_send(handle, PC_TI7383, CMD_CTS, 0, NULL));

  return 0;
}

int ti73_send_XDP_h(CalcHandle* handle, int length, uint8_t * data)
{
  printf(" PC->TI: XDP (0x%04X bytes)", length);
  TRYF(dbus_send(handle, PC_TI7383, CMD_XDP, length, data));

  return 0;
}

/*
  Skip variable
  - rej_code [in]: a rejection code
  - int [out]: an error code
 */
int ti73_send_SKP_h(CalcHandle* handle, uint8_t rej_code)
{
  TRYF(dbus_send(handle, PC_TI7383, CMD_SKP, 1, &rej_code));
  printf(" PC->TI: SKP (rejection code = %i)", rej_code);

  return 0;
}

int ti73_send_ACK_h(CalcHandle* handle)
{
  printf(" PC->TI: ACK\n");
  TRYF(dbus_send(handle, PC_TI7383, CMD_ACK, 2, NULL));

  return 0;
}

int ti73_send_ERR_h(CalcHandle* handle)
{
  printf(" PC->TI: ERR");
  TRYF(dbus_send(handle, PC_TI7383, CMD_ERR, 2, NULL));

  return 0;
}

int ti73_send_RDY_h(CalcHandle* handle)
{
  printf(" PC->TI: RDY?");
  TRYF(dbus_send(handle, PC_TI7383, CMD_RDY, 2, NULL));

  return 0;
}

int ti73_send_SCR_h(CalcHandle* handle)
{
  printf(" PC->TI: SCR");
  TRYF(dbus_send(handle, PC_TI7383, CMD_SCR, 2, NULL));

  return 0;
}

int ti73_send_KEY_h(CalcHandle* handle, uint16_t scancode)
{
	uint8_t buf[5];
  
	buf[0] = PC_TI7383;
	buf[1] = CMD_KEY;
	buf[2] = LSB(scancode);
	buf[3] = MSB(scancode);

	printf(" PC->TI: KEY");
	TRYF(ticables_cable_send(handle->cable, buf, 4));

	return 0;
}

int ti73_send_EOT_h(CalcHandle* handle)
{
  printf(" PC->TI: EOT");
  TRYF(dbus_send(handle, PC_TI7383, CMD_EOT, 2, NULL));

  return 0;
}

/* Request variable (std var header: NUL padded, fixed length) */
int ti73_send_REQ_h(CalcHandle* handle, uint16_t varsize, uint8_t vartype, const char *varname,
		  uint8_t varattr)
{
  uint8_t buffer[16] = { 0 };
  char trans[17];

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;
  memcpy(buffer + 3, varname, 8);
  pad_buffer(buffer + 3, '\0');
  buffer[11] = 0x00;
  buffer[12] = (varattr == ATTRB_ARCHIVED) ? 0x80 : 0x00;

  ticonv_varname_to_utf8_s(handle->model, varname, trans, vartype);
  printf(" PC->TI: REQ (size=0x%04X, id=%02X, name=%s, attr=%i)",
       varsize, vartype, trans, varattr);

  if (vartype != TI83p_IDLIST && vartype != TI83p_GETCERT) 
  {
    TRYF(dbus_send(handle, PC_TI7383, CMD_REQ, 11 + EXTRAS, buffer));
  } 
  else if(vartype != TI83p_GETCERT && handle->model != CALC_TI73)
  {
    TRYF(dbus_send(handle, PC_TI7383, CMD_REQ, 11, buffer));
  }
  else
  {
	  TRYF(dbus_send(handle, PC_TI73, CMD_REQ, 3, buffer));
  }

  return 0;
}

/* FLASH (special var header: size, id, flag, offset, page) */
int ti73_send_REQ2_h(CalcHandle* handle, uint16_t appsize, uint8_t apptype, const char *appname,
		   uint8_t appattr)
{
  uint8_t buffer[16] = { 0 };

  buffer[0] = LSB(appsize);
  buffer[1] = MSB(appsize);
  buffer[2] = apptype;
  memcpy(buffer + 3, appname, 8);
  pad_buffer(buffer + 3, '\0');

  printf(" PC->TI: REQ (size=0x%04X, id=%02X, name=%s)",
	  appsize, apptype, appname);
  TRYF(dbus_send(handle, PC_TI7383, CMD_REQ, 11, buffer));

  return 0;
}

/* Request to send (std var header: NUL padded, fixed length) */
int ti73_send_RTS_h(CalcHandle* handle, uint16_t varsize, uint8_t vartype, const char *varname,
		  uint8_t varattr)
{
  uint8_t buffer[16];
  char trans[9];

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;
  memcpy(buffer + 3, varname, 8);
  buffer[11] = 0x00;
  buffer[12] = (varattr == ATTRB_ARCHIVED) ? 0x80 : 0x00;

  ticonv_varname_to_utf8_s(handle->model, varname, trans, vartype);
  printf(" PC->TI: RTS (size=0x%04X, id=%02X, name=%s, attr=%i)",
       varsize, vartype, trans, varattr);

  if (vartype != TI7383_BKUP) 
  {	
	  // backup: special header
    pad_buffer(buffer + 3, '\0');
    TRYF(dbus_send(handle, PC_TI7383, CMD_RTS, 11 + EXTRAS, buffer));
  } 
  else 
  {
    TRYF(dbus_send(handle, PC_TI7383, CMD_RTS, 9, buffer));
  }

  return 0;
}

int ti73_send_VER_h(CalcHandle* handle)
{
  printf(" PC->TI: VER");
  TRYF(dbus_send(handle, PC_TI7383, CMD_VER, 2, NULL));

  return 0;
}

int ti73_send_DEL_h(CalcHandle* handle, uint16_t varsize, uint8_t vartype, const char *varname,
		  uint8_t varattr)
{
	uint8_t buffer[16] = { 0 };
	char trans[9];

	buffer[0] = LSB(varsize);
	buffer[1] = MSB(varsize);
	buffer[2] = vartype == TI83p_APPL ? 0x14 : vartype;
	memcpy(buffer + 3, varname, 8);
	pad_buffer(buffer + 3, '\0');
	buffer[11] = 0x00;

	ticonv_varname_to_utf8_s(handle->model, varname, trans, vartype);
	printf(" PC->TI: DEL (name=%s)", trans);

	TRYF(dbus_send(handle, PC_TI7383, CMD_DEL, 11, buffer));
  
	return 0;
}


int ti73_recv_VAR_h(CalcHandle* handle, uint16_t * varsize, uint8_t * vartype, char *varname,
		  uint8_t * varattr)
{
  uint8_t host, cmd;
  uint8_t *buffer = (uint8_t *)handle->priv2;
  uint16_t length;
  char trans[9];

  TRYF(dbus_recv(handle, &host, &cmd, &length, buffer));

  if (cmd == CMD_EOT)
    return ERR_EOT;		// not really an error

  if (cmd == CMD_SKP)
    return ERR_VAR_REJECTED;

  if (cmd != CMD_VAR)
    return ERR_INVALID_CMD;

  if(length < 9 || length > 13)	//if ((length != (11 + EXTRAS)) && (length != 9))
    return ERR_INVALID_PACKET;

  *varsize = buffer[0] | (buffer[1] << 8);
  *vartype = buffer[2];
  memcpy(varname, buffer + 3, 8);
  varname[8] = '\0';
  *varattr = (buffer[12] & 0x80) ? ATTRB_ARCHIVED : ATTRB_NONE;

  ticonv_varname_to_utf8_s(handle->model, varname, trans, *vartype);
  printf(" TI->PC: VAR (size=0x%04X, id=%02X, name=%s, attrb=%i)",
	  *varsize, *vartype, trans, *varattr);

  return 0;
}

/* FLASH (special var header: size, id, flag, offset, page) */
int ti73_recv_VAR2_h(CalcHandle* handle, uint16_t * length, uint8_t * type, char *name,
		   uint16_t * offset, uint16_t * page)
{
  uint8_t host, cmd;
  uint8_t *buffer = (uint8_t *)handle->priv2;
  uint16_t len;

  TRYF(dbus_recv(handle, &host, &cmd, &len, buffer));

  if (cmd == CMD_EOT)
    return ERR_EOT;		// not really an error

  if (cmd == CMD_SKP)
    return ERR_VAR_REJECTED;

  if (cmd != CMD_VAR)
    return ERR_INVALID_CMD;

  if (len != 10)
    return ERR_INVALID_PACKET;

  *length = buffer[0] | (buffer[1] << 8);
  *type = buffer[2];
  memcpy(name, buffer + 3, 3);
  name[3] = '\0';
  *offset = buffer[6] | (buffer[7] << 8);
  *page = buffer[8] | (buffer[9] << 8);
  *page &= 0xff;

  printf(" TI->PC: VAR (size=0x%04X, type=%02X, name=%s, offset=%04X, page=%02X)",
       *length, *type, name, *offset, *page);

  return 0;
}

int ti73_recv_CTS_h(CalcHandle* handle, uint16_t length)
{
  uint8_t host, cmd;
  uint16_t len;
  uint8_t *buffer = (uint8_t *)handle->priv2;

  TRYF(dbus_recv(handle, &host, &cmd, &len, buffer));

  if (cmd == CMD_SKP)
    return ERR_VAR_REJECTED;
  else if (cmd != CMD_CTS)
    return ERR_INVALID_CMD;

  if (length != len)
    return ERR_CTS_ERROR;

  printf(" TI->PC: CTS");

  return 0;
}

int ti73_recv_SKP_h(CalcHandle* handle, uint8_t * rej_code)
{
  uint8_t host, cmd;
  uint16_t length;
  uint8_t *buffer = (uint8_t *)handle->priv2;
  *rej_code = 0;

  TRYF(dbus_recv(handle, &host, &cmd, &length, buffer));

  if (cmd == CMD_CTS) 
  {
    printf("CTS");
    return 0;
  }

  if (cmd != CMD_SKP)
    return ERR_INVALID_CMD;

  printf(" TI->PC: SKP (rejection code = %i)", *rej_code = buffer[0]);

  return 0;
}

int ti73_recv_XDP_h(CalcHandle* handle, uint16_t * length, uint8_t * data)
{
  uint8_t host, cmd;

  TRYF(dbus_recv(handle, &host, &cmd, length, data));

  if (cmd != CMD_XDP)
    return ERR_INVALID_CMD;

  printf(" TI->PC: XDP (%04X bytes)\n", *length);

  return 0;
}

/*
  Receive acknowledge
  - status [in/out]: if NULL is passed, the function checks that 00 00 has
  been received. Otherwise, it put in status the received value.
  - int [out]: an error code
*/
int ti73_recv_ACK_h(CalcHandle* handle, uint16_t * status)
{
  uint8_t host, cmd;
  uint16_t length;
  uint8_t *buffer = (uint8_t *)handle->priv2;

  TRYF(dbus_recv(handle, &host, &cmd, &length, buffer));

  if (status != NULL)
    *status = length;
  else if (length != 0x0000)	// is an error code ? (=5 when app is rejected)
    return ERR_NACK;

  if (cmd != CMD_ACK)
    return ERR_INVALID_CMD;

  printf(" TI->PC: ACK\n");

  return 0;
}

int ti73_recv_RTS_h(CalcHandle* handle, uint16_t * varsize, uint8_t * vartype, char *varname,
		  uint8_t * varattr)
{
  uint8_t host, cmd;
  uint8_t *buffer = (uint8_t *)handle->priv2;
  char trans[9];

  TRYF(dbus_recv(handle, &host, &cmd, varsize, buffer));

  if (cmd != CMD_RTS)
    return ERR_INVALID_CMD;

  *varsize = buffer[0] | (buffer[1] << 8);
  *vartype = buffer[2];
  memcpy(varname, buffer + 3, 8);
  varname[8] = '\0';
  *varattr = (buffer[12] & 0x80) ? ATTRB_ARCHIVED : ATTRB_NONE;

  ticonv_varname_to_utf8_s(handle->model, varname, trans, *vartype);
  printf(" TI->PC: RTS (size=0x%04X, id=%02X, name=%s, attrb=%i)",
	  *varsize, *vartype, trans, *varattr);

  return 0;
}
