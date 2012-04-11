/*
 * TITools
 *
 * Copyright (c) 2010 Benjamin Moody
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include "titools.h"
#include "dbus_pkt.h"
#include "cmd73.h"

#define PKTSIZE 0x400

int main(int argc, char **argv)
{
  int rampage,numpages;

  tt_init(argc, argv, NULL, 0, 0, 1);

  uint8_t data[] = {0x7F, 00, 00, 0x40, 00, 0x40, 0x0C, 00};
  uint8_t *buffer;
  uint16_t bufp;
  uint16_t length;
  FILE *f;
  char *filename;
  
  if (calc_handle->model == CALC_TI84P || calc_handle->model == CALC_TI83P)
  {
      CalcInfos info;
      ticalcs_calc_get_version(calc_handle, &info);
      switch(info.hw_version)
      {
	case 1:
	case 3:
	  numpages = 0x74;
	  rampage = 0x80;
	  break;
	case 0:
	  numpages = 0x1F;
	  rampage = 0x40;
	  break;
	case 2:
	  numpages = 0x3F;
	  rampage = 0x80;
	  break;
	default:
	  goto exit; // if this happens the world might be ending
      }
      buffer = (uint8_t *)malloc(0x5000);

      filename = g_strconcat(tifiles_model_to_string(calc_handle->model), ".", "rom", NULL);
      f = fopen(filename, "wb");
      if (f == NULL)
	goto exit;
      for(data[0]=0; data[0]<=numpages; data[0]++){
	printf("Receiving page %X out of %X\n", data[0], numpages);
	printf(" PC->TI: MEM REQ\n");
	dbus_send(calc_handle, PC_TI83p, 0x6F, 8, (uint8_t *)&data);
	ti73_recv_ACK_h(calc_handle, NULL);
	ti73_recv_XDP_h(calc_handle, &length, buffer);
	ti73_send_ACK_h(calc_handle);
	if (fwrite(buffer, length, 1, f) < 1)
	  goto exit;
      }
      //fclose(f);
      //filename = g_strconcat(tifiles_model_to_string(calc_handle->model), ".", "ram", NULL);
      //f = fopen(filename, "wb");
      //if (f == NULL)
	//goto exit;
      //for(data[0]=rampage; data[0]<=rampage+1; data[0]++){
	//printf("Receiving page %X out of %X\n", data[0], rampage+1);
	//printf(" PC->TI: MEM REQ\n");
	//dbus_send(calc_handle, PC_TI83p, 0x6F, 8, (uint8_t *)&data);
	//ti73_recv_ACK_h(calc_handle, NULL);
	//ti73_recv_XDP_h(calc_handle, &length, buffer);
	//ti73_send_ACK_h(calc_handle);
	//if (fwrite(buffer, length, 1, f) < 1)
	  //goto exit;
      //}
exit:
  fclose(f);
  }

  tt_exit();
  return 0;
}
