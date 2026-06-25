#include "sensor_bar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

const SensorBarModel SENSOR_BAR_MODELS[BAR_COUNT] = {
    [BAR_QTR8RC] = {
        .type                    = BAR_QTR8RC,
        .name                    = "QTR-8RC",
        .sensor_count            = 8,
        .default_baudrate        = 115200,
        .default_threshold_black = 700,
        .default_threshold_white = 300,
        .default_timeout_ms      = 2000,
        .position_min            = 0,
        .position_max            = 7000,    /* (8-1) * 1000 */
    },
    [BAR_IM_HEM_005] = {
        .type                    = BAR_IM_HEM_005,
        .name                    = "IM-HEM-005",
        .sensor_count            = 16,
        .default_baudrate        = 115200,
        .default_threshold_black = 700,
        .default_threshold_white = 300,
        .default_timeout_ms      = 2000,
        .position_min            = -7500,   /* extremo izquierdo */
        .position_max            = 7500,    /* extremo derecho   */
    },
};

void sensor_bar_init(SensorBar *bar, BarType type) {
    if (type < 0 || type >= BAR_COUNT) {
        fprintf(stderr,
            "\nError fatal: tipo de barra invalido (%d).\n"
            "Verifique o elimine config.ini y vuelva a ejecutar.\n", (int)type);
        exit(1);
    }

    memset(bar, 0, sizeof(*bar));
    bar->model           = &SENSOR_BAR_MODELS[type];
    bar->threshold_black = bar->model->default_threshold_black;
    bar->threshold_white = bar->model->default_threshold_white;
    bar->timeout_ms      = bar->model->default_timeout_ms;
    bar->has_prev        = 0;
    bar->has_calibration = 0;
}

int sensor_bar_parse_message(SensorBar *bar, const char *msg) {
    /* Ignora lineas de cabecera/calibracion que empiezan con letra */
    if (!isdigit((unsigned char)msg[0])) return 0;

    int n = bar->model->sensor_count;
    for (int i = 0; i < n; i++)
        bar->prev_values[i] = bar->values[i];
    bar->has_prev = 1;

    /* Formato: v1\tv2\t...\tvN\tposition */
    char  copy[256];
    strncpy(copy, msg, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    int    i     = 0;
    char  *token = strtok(copy, "\t");
    while (token && i < n) {
        bar->values[i++] = atoi(token);
        token = strtok(NULL, "\t");
    }
    if (i < n) return 0;

    if (token) bar->position = atoi(token);
    return 1;
}

int sensor_bar_parse_calibration(SensorBar *bar, const char *msg) {
    int is_min = (strncmp(msg, "Min:", 4) == 0);
    int is_max = (strncmp(msg, "Max:", 4) == 0);
    if (!is_min && !is_max) return 0;

    /* Salta el prefijo "Min: " o "Max: " */
    const char *p = msg + 4;
    while (*p == ' ') p++;

    char copy[256];
    strncpy(copy, p, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    int   n     = bar->model->sensor_count;
    int   i     = 0;
    char *token = strtok(copy, "\t");
    while (token && i < n) {
        int value = atoi(token);
        if (is_min) bar->calib_min[i] = value;
        else        bar->calib_max[i] = value;
        i++;
        token = strtok(NULL, "\t");
    }

    if (is_max) {
        bar->has_calibration = 1;
    }
    return 1;
}
