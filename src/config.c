#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void config_defaults(AppConfig *cfg) {
    const SensorBarModel *m = &SENSOR_BAR_MODELS[BAR_QTR8RC];
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->port, "COM3", CONFIG_PORT_LEN - 1);
    cfg->baudrate        = m->default_baudrate;
    cfg->bar_type        = BAR_QTR8RC;
    cfg->threshold_black = m->default_threshold_black;
    cfg->threshold_white = m->default_threshold_white;
    cfg->timeout_ms      = m->default_timeout_ms;
    strncpy(cfg->board_fqbn, "arduino:avr:nano:cpu=atmega328old", sizeof(cfg->board_fqbn) - 1);
    strncpy(cfg->arduino_cli_path, "arduino-cli", sizeof(cfg->arduino_cli_path) - 1);
    strncpy(cfg->avrdude_path, "avrdude", sizeof(cfg->avrdude_path) - 1);
    strncpy(cfg->backup_hex_path, "backup_fw.hex", sizeof(cfg->backup_hex_path) - 1);
}

static void trim(char *s) {
    char *end = s + strlen(s) - 1;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

int config_load(AppConfig *cfg, const char *path) {
    config_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\0')
            continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if      (strcmp(key, "port")            == 0) strncpy(cfg->port, val, CONFIG_PORT_LEN - 1);
        else if (strcmp(key, "baudrate")         == 0) cfg->baudrate        = atoi(val);
        else if (strcmp(key, "bar_type")         == 0) {
            int t = atoi(val);
            if (t < 0 || t >= BAR_COUNT) {
                fprintf(stderr,
                    "\nError: bar_type=%d en config.ini es invalido (rango valido: 0-%d).\n"
                    "Se usara el tipo por defecto (QTR-8RC).\n", t, BAR_COUNT - 1);
                cfg->bar_type = BAR_QTR8RC;
            } else {
                cfg->bar_type = (BarType)t;
            }
        }
        else if (strcmp(key, "threshold_black")  == 0) cfg->threshold_black = atoi(val);
        else if (strcmp(key, "threshold_white")  == 0) cfg->threshold_white = atoi(val);
        else if (strcmp(key, "timeout_ms")       == 0) cfg->timeout_ms      = atoi(val);
        else if (strcmp(key, "board_fqbn")       == 0) strncpy(cfg->board_fqbn, val, sizeof(cfg->board_fqbn) - 1);
        else if (strcmp(key, "arduino_cli_path") == 0) strncpy(cfg->arduino_cli_path, val, sizeof(cfg->arduino_cli_path) - 1);
        else if (strcmp(key, "avrdude_path")     == 0) strncpy(cfg->avrdude_path, val, sizeof(cfg->avrdude_path) - 1);
        else if (strcmp(key, "backup_hex_path")  == 0) strncpy(cfg->backup_hex_path, val, sizeof(cfg->backup_hex_path) - 1);
    }
    fclose(f);
    return 1;
}

int config_save(const AppConfig *cfg, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "[serial]\n");
    fprintf(f, "port=%s\n",     cfg->port);
    fprintf(f, "baudrate=%d\n", cfg->baudrate);
    fprintf(f, "\n[bar]\n");
    fprintf(f, "bar_type=%d\n",        (int)cfg->bar_type);
    fprintf(f, "threshold_black=%d\n", cfg->threshold_black);
    fprintf(f, "threshold_white=%d\n", cfg->threshold_white);
    fprintf(f, "timeout_ms=%d\n",      cfg->timeout_ms);
    fprintf(f, "\n[tools]\n");
    fprintf(f, "board_fqbn=%s\n",       cfg->board_fqbn);
    fprintf(f, "arduino_cli_path=%s\n", cfg->arduino_cli_path);
    fprintf(f, "avrdude_path=%s\n",     cfg->avrdude_path);
    fprintf(f, "backup_hex_path=%s\n",  cfg->backup_hex_path);

    fclose(f);
    return 1;
}
