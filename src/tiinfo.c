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

int main(int argc, char **argv)
{
  CalcInfos info;
  uint32_t ram, flash;
  int e;

  tt_init(argc, argv, NULL, 0, 0, 1);

  g_print("Model: %s\n", ticalcs_model_to_string(calc_model));

  if (ticalcs_calc_features(calc_handle) & OPS_VERSION) {
    if ((e = ticalcs_calc_get_version(calc_handle, &info))) {
      tt_print_error(e, "unable to get version info");
      tt_exit();
      return 1;
    }

    if (info.mask & INFOS_PRODUCT_NAME)
      g_print("Product name:      %s\n", info.product_name);
    if (info.mask & INFOS_PRODUCT_ID)
      g_print("ID:                %s\n", info.product_id);
    if (info.mask & INFOS_HW_VERSION)
      g_print("Hardware version:  %d\n", info.hw_version);
    if (info.mask & INFOS_BOOT_VERSION)
      g_print("Boot code version: %s\n", info.boot_version);
    if (info.mask & INFOS_BOOT2_VERSION)
      g_print("Boot2 version:     %s\n", info.boot2_version);
    if (info.mask & INFOS_OS_VERSION)
      g_print("OS version:        %s\n", info.os_version);

    if (info.mask & INFOS_LANG_ID)
      g_print("Language:          %d\n", info.language_id);
    if (info.mask & INFOS_SUB_LANG_ID)
      g_print("Sub-language:      %d\n", info.sub_lang_id);
    if (info.mask & INFOS_DEVICE_TYPE)
      g_print("Device type:       0x%x\n", info.device_type);
    if (info.mask & INFOS_RAM_PHYS)
      g_print("Physical RAM:      %" G_GUINT64_FORMAT "\n", info.ram_phys);
    if (info.mask & INFOS_RAM_USER)
      g_print("User RAM:          %" G_GUINT64_FORMAT "\n", info.ram_user);
    if (info.mask & INFOS_RAM_FREE)
      g_print("Free RAM:          %" G_GUINT64_FORMAT "\n", info.ram_free);
    if (info.mask & INFOS_FLASH_PHYS)
      g_print("Physical Flash:    %" G_GUINT64_FORMAT "\n", info.flash_phys);
    if (info.mask & INFOS_FLASH_USER)
      g_print("User Flash:        %" G_GUINT64_FORMAT "\n", info.flash_user);
    if (info.mask & INFOS_FLASH_FREE)
      g_print("Free Flash:        %" G_GUINT64_FORMAT "\n", info.flash_free);
    if (info.mask & INFOS_LCD_WIDTH)
      g_print("LCD width:         %d\n", info.lcd_width);
    if (info.mask & INFOS_LCD_HEIGHT)
      g_print("LCD height:        %d\n", info.lcd_height);
    if (info.mask & INFOS_BPP)
      g_print("LCD bit depth:     %d\n", info.bits_per_pixel);
    if (info.mask & INFOS_BATTERY)
      g_print("Battery:           %s\n", info.battery ? "good" : "low");
    if (info.mask & INFOS_RUN_LEVEL)
      g_print("Run level:         %d\n", info.run_level);
    if (info.mask & INFOS_CLOCK_SPEED)
      g_print("Clock speed:       %d\n", info.clock_speed);
  }
  else if (ticalcs_calc_features(calc_handle) & FTS_MEMFREE) {
    if ((e = ticalcs_calc_get_memfree(calc_handle, &ram, &flash))) {
      tt_print_error(e, "unable to get free memory");
      tt_exit();
      return 1;
    }

    if (ram != (uint32_t) -1)
      g_print("Free RAM:          %d\n", ram);
    if (flash != (uint32_t) -1)
      g_print("Free Flash:        %d\n", flash);
  }

  tt_exit();
  return 0;
}
