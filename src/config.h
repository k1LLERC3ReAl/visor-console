#ifndef CONFIG_H
#define CONFIG_H

#include "sensor_bar.h"

#define CONFIG_FILE     "config.ini"
#define CONFIG_PORT_LEN 16

typedef struct {
    char    port[CONFIG_PORT_LEN];
    int     baudrate;
    BarType bar_type;
    int     threshold_black;
    int     threshold_white;
    int     timeout_ms;
    char    board_fqbn[64];
    char    arduino_cli_path[256];
    char    avrdude_path[256];
    char    backup_hex_path[256];
} AppConfig;

void config_defaults(AppConfig *cfg);
int  config_load(AppConfig *cfg, const char *path);
int  config_save(const AppConfig *cfg, const char *path);

#endif
