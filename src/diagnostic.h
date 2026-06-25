#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include "sensor_bar.h"

typedef enum {
    DIAG_UNKNOWN = 0,
    DIAG_BLANCO,
    DIAG_GRIS,
    DIAG_NEGRO,
    DIAG_FAIL_TIMEOUT,
} DiagStatus;

typedef struct {
    DiagStatus status[SENSOR_MAX_COUNT];
    int        negro_count;
    int        gris_count;
    int        blanco_count;
    int        timeout_count;
    int        sensor_count;
} DiagReport;

void        diagnostic_run(const SensorBar *bar, DiagReport *report);
void        diagnostic_timeout(DiagReport *report, int sensor_count);
const char *diagnostic_status_str(DiagStatus s);

#endif
