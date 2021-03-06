/********************
 * Lulzbot_Extras.h *
 ********************/

/****************************************************************************
 *   Written By Marcio Teixeira 2018 - Aleph Objects, Inc.                  *
 *                                                                          *
 *   This program is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation, either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   To view a copy of the GNU General Public License, go to the following  *
 *   location: <http://www.gnu.org/licenses/>.                              *
 ****************************************************************************/

#pragma once
#include <stdbool.h>

/******************************** EXTRA FEATURES *******************************/

#ifdef __cplusplus
  extern "C" void on_reflash();
#else
  void on_reflash(void);
#endif

#if ENABLED(LULZBOT_TOUCH_UI)
  #define LULZBOT_ON_REFLASH on_reflash();
#endif

/******************************** EMI MITIGATION *******************************/

#if ENABLED(LULZBOT_EMI_MITIGATION)
  void emi_shutoff_pins(void);
  void enable_emi_pins(const bool);
#endif

/**************************** Z-OFFSET AUTO-SAVE  ******************************/

/* Historically, the Lulzbot firmware would save the Z-Offset into the EEPROM
 * each time it is changed. The latest Marlin made this more difficult since they
 * added a CRC to the EEPROM. We work around this by bracketing the EEPROM_READ
 * and EEPROM_WRITE routines such that the CRC ignores the Z-Offset value. That
 * code also captures the eeprom_index so we can write only that value later.
 */
#define HAS_Z_AUTO_SAVE    HAS_BED_PROBE && HAS_LCD_MENU && EITHER(EEPROM_SETTINGS, SD_FIRMWARE_UPDATE)

#if HAS_Z_AUTO_SAVE
  #ifdef __cplusplus
    class AutoSaveZOffset {
      private:
        static int eeprom_offset;
      public:
        static void offset(int offset) {eeprom_offset = offset;}
        static void store();
    };
  #endif

  /* The following goes in "configuration_store.h" */
  #define LULZBOT_SAVE_ZOFFSET_TO_EEPROM_DECL static AutoSaveZOffset aszo;

  /* The following goes in "configuration_store.cpp", before and after
   * "EEPROM_WRITE(zprobe_zoffset)" and "EEPROM_READ(zprobe_zoffset)" */
  #define LULZBOT_EEPROM_BEFORE_ZOFFSET \
    const uint16_t saved_crc = working_crc; \
    aszo.offset(eeprom_index);

  #define LULZBOT_EEPROM_AFTER_ZOFFSET working_crc = saved_crc;

  /* The following goes in "ultralcd.cpp" in "lcd_babystep_zoffset" */
  #define LULZBOT_SAVE_ZOFFSET_TO_EEPROM settings.aszo.store();
#endif

/******************************** PROBE QUALITY CHECK *************************/

#if defined(AUTO_BED_LEVELING_LINEAR)
  #ifdef __cplusplus
    class BedLevelingReport {
      private:
        float x[4], y[4], z[4];

      public:
        void point(uint8_t i, float _x, float _y, float _z) {
          x[i] = _x;
          y[i] = _y;
          z[i] = _z;
        }

        void report();
    };
  #endif

  #define LULZBOT_BED_LEVELING_DECL BedLevelingReport blr;
  #define LULZBOT_BED_LEVELING_POINT(i,x,y,z) blr.point(i,x,y,z);
  #define LULZBOT_BED_LEVELING_SUMMARY blr.report();
#endif

/**************************** SD SUPPORT DEBUGGING ***************************/

#if defined(LULZBOT_SDSUPPORT_DEBUG)
  #define LULZBOT_SDCARD_CHECK_BYTE(n) \
    static bool spi_error = false; \
    if(n != -1 && !isprint(n) && n != '\n' && n != '\r') spi_error = true;

  #define LULZBOT_SDCARD_COMMAND_DONE(cmd) \
    if(spi_error) { \
      SERIAL_ECHOLNPAIR("Likely SPI read error: ", cmd); \
      spi_error = false; \
    }
#endif

/**************************** USB THUMBDRIVE SUPPORT ***************************/

#if defined(USB_FLASH_DRIVE_SUPPORT)
  #define USB_INTR_PIN                  SD_DETECT_PIN
#endif

#if defined(USE_UHS3_USB)
  #define LULZBOT_USB_NO_TEST_UNIT_READY // Required for removable media adapter
  #define LULZBOT_SKIP_PAGE3F // Required for IOGEAR media adapter
  #define LULZBOT_USB_HOST_MANUAL_POLL // Optimization to shut off IRQ automatically

  // Speed up I/O operations using Marlin functions
  #define LULZBOT_UHS_WRITE_SS(v)    WRITE(USB_CS_PIN, v)
  #define LULZBOT_UHS_READ_IRQ()     READ(USB_INTR_PIN)

  #if defined(LULZBOT_USB_HOST_MANUAL_POLL)
    #define LULZBOT_ENABLE_FRAME_IRQ(en) enable_frame_irq(en)
    #define LULZBOT_ENABLE_FRAME_IRQ_DEFN \
      volatile bool frame_irq_enabled = false; \
      bool enable_frame_irq(bool enable) { \
        const bool prev_state = frame_irq_enabled; \
        if(prev_state != enable) { \
          if(enable) \
            regWr(rHIEN, regRd(rHIEN) |  bmFRAMEIE); \
          else \
            regWr(rHIEN, regRd(rHIEN) & ~bmFRAMEIE); \
          frame_irq_enabled = enable; \
        } \
        return prev_state; \
      }
    #define LULZBOT_USB_IDLE_TASK \
      if(usb_task_state == UHS_USB_HOST_STATE_RUNNING) { \
        noInterrupts(); \
        for(uint8_t x = 0; x < UHS_HOST_MAX_INTERFACE_DRIVERS; x++) { \
          if(devConfig[x] && devConfig[x]->bPollEnable) { \
            devConfig[x]->Poll(); \
          } \
        } \
        interrupts(); \
      }
    #define LULZBOT_FRAME_IRQ_SAVE(en) const bool saved_irq_state = enable_frame_irq(en)
    #define LULZBOT_FRAME_IRQ_RESTORE() enable_frame_irq(saved_irq_state)
  #else
    #define LULZBOT_ENABLE_FRAME_IRQ(en)
    #define LULZBOT_ENABLE_FRAME_IRQ_DEFN
    #define LULZBOT_FRAME_IRQ_SAVE(en)
    #define LULZBOT_FRAME_IRQ_RESTORE()
    #define LULZBOT_USB_IDLE_TASK
  #endif
#endif

/**************************** LANGUAGE CHANGES ***************************/

#if EITHER(ULTIPANEL, EXTENSIBLE_UI)
  #if defined(LULZBOT_LCD_MACHINE_NAME)
    #if defined(LULZBOT_LIGHTWEIGHT_UI)
      #define WELCOME_MSG _UxGT(LULZBOT_LCD_MACHINE_NAME " ready.")
    #else
      #define WELCOME_MSG _UxGT("LulzBot " LULZBOT_LCD_MACHINE_NAME " ready.")
    #endif
  #endif

  // Change wording on a couple menu items
  #define MSG_ERR_PROBING_FAILED            _UxGT("Autolevel failed")
  #define MSG_MOVE_E                        _UxGT("Extruder ") // Add space to extruder string
  #define MSG_INFO_PRINTER_MENU             _UxGT("Firmware Version") // About Printer -> Firmware Version

  #define MSG_FILAMENT_CHANGE_HEADER_PAUSE  _UxGT("Changing Filament")
  #define MSG_FILAMENT_CHANGE_RESUME_1      _UxGT("")
  #define MSG_FILAMENT_CHANGE_RESUME_2      _UxGT("Please wait.")

  #define MSG_FILAMENT_CHANGE_OPTION_HEADER _UxGT("")
  #define MSG_FILAMENT_CHANGE_OPTION_RESUME _UxGT("End filament change")
#endif

#define LULZBOT_EXTRUDER_STR "Hotend"
