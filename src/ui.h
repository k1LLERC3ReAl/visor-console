#ifndef UI_H
#define UI_H

#include "serial.h"
#include "sensor_bar.h"
#include "diagnostic.h"
#include "config.h"

void ui_clear(void);
void ui_pause(void);

int  ui_main_menu(const AppConfig *cfg);
void ui_select_port(AppConfig *cfg);
void ui_select_baudrate(AppConfig *cfg);
void ui_select_board(AppConfig *cfg);
void ui_select_bar_type(AppConfig *cfg);
void ui_configure_thresholds(AppConfig *cfg, SensorBar *bar);
void ui_reading_screen(SerialPort *sp, SensorBar *bar, AppConfig *cfg);

#endif
