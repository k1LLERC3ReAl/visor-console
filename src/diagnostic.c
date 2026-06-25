#include "diagnostic.h"

void diagnostic_run(const SensorBar *bar, DiagReport *report) {
    int n = bar->model->sensor_count;
    report->negro_count   = 0;
    report->gris_count    = 0;
    report->blanco_count  = 0;
    report->timeout_count = 0;
    report->sensor_count  = n;

    for (int i = 0; i < n; i++) {
        int v = bar->values[i];
        if (v >= bar->threshold_black)
            report->status[i] = DIAG_NEGRO;
        else if (v < bar->threshold_white)
            report->status[i] = DIAG_BLANCO;
        else
            report->status[i] = DIAG_GRIS;

        switch (report->status[i]) {
            case DIAG_NEGRO:  report->negro_count++;  break;
            case DIAG_GRIS:   report->gris_count++;   break;
            case DIAG_BLANCO: report->blanco_count++;  break;
            default: break;
        }
    }
}

void diagnostic_timeout(DiagReport *report, int sensor_count) {
    report->negro_count   = 0;
    report->gris_count    = 0;
    report->blanco_count  = 0;
    report->timeout_count = sensor_count;
    report->sensor_count  = sensor_count;
    for (int i = 0; i < sensor_count; i++)
        report->status[i] = DIAG_FAIL_TIMEOUT;
}

const char *diagnostic_status_str(DiagStatus s) {
    switch (s) {
        case DIAG_NEGRO:        return "NEGRO";
        case DIAG_GRIS:         return "GRIS";
        case DIAG_BLANCO:       return "BLANCO";
        case DIAG_FAIL_TIMEOUT: return "TOUT";
        default:                return "---";
    }
}
