#include <stdio.h>
#include <string.h>
#include "serial.h"
#include "sensor_bar.h"
#include "ui.h"
#include "config.h"

int main(void) {
    AppConfig cfg;
    config_load(&cfg, CONFIG_FILE);

    SensorBar bar;
    sensor_bar_init(&bar, cfg.bar_type);
    bar.threshold_black = cfg.threshold_black;
    bar.threshold_white = cfg.threshold_white;
    bar.timeout_ms      = cfg.timeout_ms;

    SerialPort sp;
    memset(&sp, 0, sizeof(sp));
    sp.handle = INVALID_HANDLE_VALUE;

    int running = 1;
    while (running) {
        int choice = ui_main_menu(&cfg);
        switch (choice) {
            case 1:
                ui_select_port(&cfg);
                break;

            case 2:
                ui_select_baudrate(&cfg);
                break;

            case 3:
                ui_select_board(&cfg);
                break;

            case 4:
                ui_select_bar_type(&cfg);
                sensor_bar_init(&bar, cfg.bar_type);
                bar.threshold_black = cfg.threshold_black;
                bar.threshold_white = cfg.threshold_white;
                bar.timeout_ms      = cfg.timeout_ms;
                break;

            case 5:
                ui_configure_thresholds(&cfg, &bar);
                break;

            case 6:
                if (!cfg.port[0]) {
                    printf("\n  Error: seleccione primero un puerto.\n");
                    ui_pause();
                    break;
                }
                if (serial_open(&sp, cfg.port, cfg.baudrate)) {
                    ui_reading_screen(&sp, &bar, &cfg);
                    serial_close(&sp);
                } else {
                    printf("\n  Error: no se pudo abrir %s (baudrate %d).\n",
                           cfg.port, cfg.baudrate);
                    ui_pause();
                }
                break;

            case 7:
                if (config_save(&cfg, CONFIG_FILE))
                    printf("\n  Configuracion guardada en %s\n", CONFIG_FILE);
                else
                    printf("\n  Error al guardar configuracion.\n");
                ui_pause();
                break;

            case 0:
                running = 0;
                break;

            default:
                break;
        }
    }

    serial_close(&sp);
    return 0;
}
