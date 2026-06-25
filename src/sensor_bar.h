#ifndef SENSOR_BAR_H
#define SENSOR_BAR_H

#define SENSOR_MAX_COUNT 16

typedef enum {
    BAR_QTR8RC = 0,
    BAR_IM_HEM_005,
    BAR_COUNT
} BarType;

typedef struct {
    BarType type;
    char    name[32];
    int     sensor_count;
    int     default_baudrate;
    int     default_threshold_black;  /* valor >= este -> NEGRO  */
    int     default_threshold_white;  /* valor <  este -> BLANCO */
    int     default_timeout_ms;
    int     position_min;             /* valor minimo de posicion */
    int     position_max;             /* valor maximo de posicion */
} SensorBarModel;

extern const SensorBarModel SENSOR_BAR_MODELS[BAR_COUNT];

typedef struct {
    const SensorBarModel *model;
    int values[SENSOR_MAX_COUNT];
    int prev_values[SENSOR_MAX_COUNT];
    int has_prev;
    int position;
    /* calibracion recibida del Arduino */
    int calib_min[SENSOR_MAX_COUNT];
    int calib_max[SENSOR_MAX_COUNT];
    int has_calibration;
    /* umbrales activos */
    int threshold_black;
    int threshold_white;
    int timeout_ms;
} SensorBar;

void sensor_bar_init(SensorBar *bar, BarType type);
int  sensor_bar_parse_message(SensorBar *bar, const char *msg);
int  sensor_bar_parse_calibration(SensorBar *bar, const char *msg);

#endif
