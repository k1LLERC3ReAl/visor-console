#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "serial.h"
#include "sensor_bar.h"
#include "diagnostic.h"
#include "ui.h"
#include "config.h"
#include "upload.h"
#include "boards.h"
#include "utils.h"

/* ── JSON mode ──────────────────────────────────────────────────────────── */

static void jstr(const char *s) {
    putchar('"');
    for (; *s; s++) {
        if      (*s == '"')  printf("\\\"");
        else if (*s == '\\') printf("\\\\");
        else if (*s == '\n') printf("\\n");
        else if (*s != '\r') putchar(*s);
    }
    putchar('"');
}

static void json_list_ports(void) {
    char ports[SERIAL_MAX_PORTS][SERIAL_PORT_NAME_LEN];
    int  n = serial_detect_ports(ports, SERIAL_MAX_PORTS);
    printf("{\"t\":\"ports\",\"list\":[");
    for (int i = 0; i < n; i++) {
        if (i) printf(",");
        jstr(ports[i]);
    }
    printf("]}\n");
    fflush(stdout);
}

static void json_get_config(const AppConfig *cfg) {
    printf("{\"t\":\"config\","
           "\"port\":\"%s\","
           "\"baudrate\":%d,"
           "\"bar_type\":%d,"
           "\"threshold_black\":%d,"
           "\"threshold_white\":%d,"
           "\"timeout_ms\":%d,"
           "\"board_fqbn\":\"%s\","
           "\"bar_name\":\"%s\","
           "\"sensors\":%d,"
           "\"pos_min\":%d,"
           "\"pos_max\":%d}\n",
           cfg->port, cfg->baudrate, (int)cfg->bar_type,
           cfg->threshold_black, cfg->threshold_white, cfg->timeout_ms,
           cfg->board_fqbn,
           SENSOR_BAR_MODELS[cfg->bar_type].name,
           SENSOR_BAR_MODELS[cfg->bar_type].sensor_count,
           SENSOR_BAR_MODELS[cfg->bar_type].position_min,
           SENSOR_BAR_MODELS[cfg->bar_type].position_max);
    fflush(stdout);
}

static void json_set(AppConfig *cfg, const char *key, const char *value) {
    if (strcmp(key, "port") == 0) {
        strncpy(cfg->port, value, sizeof(cfg->port) - 1);
    } else if (strcmp(key, "baudrate") == 0) {
        cfg->baudrate = atoi(value);
    } else if (strcmp(key, "bar_type") == 0) {
        int t = atoi(value);
        if (t >= 0 && t < BAR_COUNT) {
            cfg->bar_type        = (BarType)t;
            const SensorBarModel *m = &SENSOR_BAR_MODELS[t];
            cfg->baudrate        = m->default_baudrate;
            cfg->threshold_black = m->default_threshold_black;
            cfg->threshold_white = m->default_threshold_white;
            cfg->timeout_ms      = m->default_timeout_ms;
        }
    } else if (strcmp(key, "threshold_black") == 0) {
        cfg->threshold_black = atoi(value);
    } else if (strcmp(key, "threshold_white") == 0) {
        cfg->threshold_white = atoi(value);
    } else if (strcmp(key, "timeout_ms") == 0) {
        cfg->timeout_ms = atoi(value);
    } else if (strcmp(key, "board_fqbn") == 0) {
        strncpy(cfg->board_fqbn, value, sizeof(cfg->board_fqbn) - 1);
    }
    if (config_save(cfg, CONFIG_FILE))
        printf("{\"t\":\"ok\"}\n");
    else
        printf("{\"t\":\"error\",\"msg\":\"No se pudo guardar config\"}\n");
    fflush(stdout);
}

static void json_emit_sensor(const SensorBar *bar, const DiagReport *report) {
    int n = bar->model->sensor_count;
    printf("{\"t\":\"sensor\",\"v\":[");
    for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", bar->values[i]); }
    printf("],\"s\":[");
    for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", (int)report->status[i]); }
    printf("],\"pos\":%d,\"negro\":%d,\"gris\":%d,\"blanco\":%d,\"tout\":%d",
           bar->position, report->negro_count, report->gris_count,
           report->blanco_count, report->timeout_count);
    if (bar->has_calibration) {
        printf(",\"min\":[");
        for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", bar->calib_min[i]); }
        printf("],\"max\":[");
        for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", bar->calib_max[i]); }
        printf("]");
    }
    printf("}\n");
    fflush(stdout);
}

static void json_emit_calib(const SensorBar *bar) {
    int n = bar->model->sensor_count;
    printf("{\"t\":\"calib\",\"min\":[");
    for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", bar->calib_min[i]); }
    printf("],\"max\":[");
    for (int i = 0; i < n; i++) { if (i) printf(","); printf("%d", bar->calib_max[i]); }
    printf("]}\n");
    fflush(stdout);
}

static void json_monitor(const AppConfig *cfg) {
    SensorBar bar;
    sensor_bar_init(&bar, cfg->bar_type);
    bar.threshold_black = cfg->threshold_black;
    bar.threshold_white = cfg->threshold_white;
    bar.timeout_ms      = cfg->timeout_ms;

    SerialPort sp;
    memset(&sp, 0, sizeof(sp));
    sp.handle = INVALID_HANDLE_VALUE;

    if (!serial_open(&sp, cfg->port, cfg->baudrate)) {
        printf("{\"t\":\"error\",\"msg\":\"No se pudo abrir %s\"}\n", cfg->port);
        fflush(stdout);
        return;
    }
    printf("{\"t\":\"connected\",\"port\":\"%s\",\"baudrate\":%d,"
           "\"bar\":\"%s\",\"sensors\":%d,\"pos_min\":%d,\"pos_max\":%d}\n",
           sp.name, sp.baudrate, bar.model->name, bar.model->sensor_count,
           bar.model->position_min, bar.model->position_max);
    fflush(stdout);

    char      msg_buf[SERIAL_BUF_LEN];
    DiagReport report;
    memset(&report, 0, sizeof(report));
    uint64_t last_rx  = utils_millis();
    int      timed_out = 0;

    while (1) {
        int ret = serial_read_line(&sp, msg_buf, sizeof(msg_buf), 500);
        if (ret > 0) {
            if (sensor_bar_parse_message(&bar, msg_buf)) {
                last_rx   = utils_millis();
                timed_out = 0;
                diagnostic_run(&bar, &report);
                json_emit_sensor(&bar, &report);
            } else if (sensor_bar_parse_calibration(&bar, msg_buf)) {
                json_emit_calib(&bar);
            }
        }
        if (!timed_out && (int)(utils_millis() - last_rx) >= cfg->timeout_ms) {
            timed_out = 1;
            diagnostic_timeout(&report, bar.model->sensor_count);
            json_emit_sensor(&bar, &report);
        }
    }
    serial_close(&sp);
}

static void json_diagnostic(const AppConfig *cfg) {
    SensorBar bar;
    sensor_bar_init(&bar, cfg->bar_type);
    bar.threshold_black = cfg->threshold_black;
    bar.threshold_white = cfg->threshold_white;
    bar.timeout_ms      = cfg->timeout_ms;

    const char *sketch_dir  = NULL;
    const char *sketch_name = NULL;
    switch (bar.model->type) {
        case BAR_QTR8RC:
            sketch_dir  = "arduino/qtr8rc_visor";
            sketch_name = "qtr8rc_visor";
            break;
        case BAR_IM_HEM_005:
            sketch_dir  = "arduino/im_hem_005";
            sketch_name = "im_hem_005";
            break;
        default:
            printf("{\"t\":\"error\",\"msg\":\"Tipo de barra desconocido\"}\n");
            fflush(stdout);
            return;
    }

    /* Paso 1: Backup */
    printf("{\"t\":\"step\",\"n\":1,\"total\":5,\"msg\":\"Guardando firmware original...\"}\n");
    fflush(stdout);
    int backup_ok = upload_backup_firmware(cfg->avrdude_path, cfg->port,
                                           cfg->board_fqbn, cfg->backup_hex_path);
    printf("{\"t\":\"step_done\",\"n\":1,\"ok\":%s}\n", backup_ok ? "true" : "false");
    fflush(stdout);

    /* Paso 2: Patch baudrate */
    printf("{\"t\":\"step\",\"n\":2,\"total\":5,\"msg\":\"Ajustando baudrate del sketch...\"}\n");
    fflush(stdout);
    char src_ino[512], dst_ino[512];
    snprintf(src_ino, sizeof(src_ino), "%s/%s.ino", sketch_dir, sketch_name);
    snprintf(dst_ino, sizeof(dst_ino), "%s_patched.ino", sketch_dir);
    int patch_ok = upload_patch_baudrate(src_ino, dst_ino, cfg->baudrate);
    printf("{\"t\":\"step_done\",\"n\":2,\"ok\":%s}\n", patch_ok ? "true" : "false");
    fflush(stdout);
    if (!patch_ok) {
        printf("{\"t\":\"error\",\"msg\":\"No se pudo ajustar el baudrate\"}\n");
        fflush(stdout);
        return;
    }

    /* Paso 3: Compilar y flashear */
    printf("{\"t\":\"step\",\"n\":3,\"total\":5,\"msg\":\"Compilando y subiendo sketch...\"}\n");
    fflush(stdout);
    int flash_ok = upload_flash_sketch(cfg->arduino_cli_path, sketch_dir,
                                       cfg->board_fqbn, cfg->port);
    printf("{\"t\":\"step_done\",\"n\":3,\"ok\":%s}\n", flash_ok ? "true" : "false");
    fflush(stdout);
    if (!flash_ok) {
        printf("{\"t\":\"error\",\"msg\":\"Fallo la compilacion o carga del sketch\"}\n");
        fflush(stdout);
        return;
    }

    /* Paso 4: Esperar reinicio */
    printf("{\"t\":\"step\",\"n\":4,\"total\":5,\"msg\":\"Esperando reinicio del Arduino...\"}\n");
    fflush(stdout);
    Sleep(2000);
    printf("{\"t\":\"step_done\",\"n\":4,\"ok\":true}\n");
    fflush(stdout);

    /* Paso 5: Monitoreo (corre hasta que el proceso sea terminado) */
    printf("{\"t\":\"step\",\"n\":5,\"total\":5,\"msg\":\"Iniciando lectura de sensores...\"}\n");
    fflush(stdout);
    Sleep(1000);

    SerialPort sp;
    memset(&sp, 0, sizeof(sp));
    sp.handle = INVALID_HANDLE_VALUE;

    if (!serial_open(&sp, cfg->port, cfg->baudrate)) {
        printf("{\"t\":\"error\",\"msg\":\"No se pudo abrir el puerto para monitoreo\"}\n");
        fflush(stdout);
        return;
    }

    printf("{\"t\":\"monitoring\",\"sensors\":%d,\"pos_min\":%d,\"pos_max\":%d}\n",
           bar.model->sensor_count, bar.model->position_min, bar.model->position_max);
    fflush(stdout);

    char       msg_buf[SERIAL_BUF_LEN];
    DiagReport report;
    memset(&report, 0, sizeof(report));
    uint64_t last_rx   = utils_millis();
    int      timed_out = 0;

    while (1) {
        int ret = serial_read_line(&sp, msg_buf, sizeof(msg_buf), 500);
        if (ret > 0) {
            if (sensor_bar_parse_message(&bar, msg_buf)) {
                last_rx   = utils_millis();
                timed_out = 0;
                diagnostic_run(&bar, &report);
                json_emit_sensor(&bar, &report);
            } else {
                sensor_bar_parse_calibration(&bar, msg_buf);
            }
        }
        if (!timed_out && (int)(utils_millis() - last_rx) >= cfg->timeout_ms) {
            timed_out = 1;
            diagnostic_timeout(&report, bar.model->sensor_count);
            json_emit_sensor(&bar, &report);
        }
    }
    serial_close(&sp);
}

static void json_restore(const AppConfig *cfg) {
    printf("{\"t\":\"step\",\"n\":1,\"total\":1,\"msg\":\"Restaurando firmware original...\"}\n");
    fflush(stdout);
    int ok = upload_restore_firmware(cfg->avrdude_path, cfg->port,
                                     cfg->board_fqbn, cfg->backup_hex_path);
    if (ok) {
        printf("{\"t\":\"done\",\"ok\":true}\n");
    } else {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "avrdude -v -p m328p -c arduino -P %s -D -U flash:w:%s:i",
                 cfg->port, cfg->backup_hex_path);
        printf("{\"t\":\"done\",\"ok\":false,\"restore_cmd\":");
        jstr(cmd);
        printf("}\n");
    }
    fflush(stdout);
}

/* ── Modo consola original ──────────────────────────────────────────────── */

int main(int argc, char **argv) {
    /* Modo JSON para interfaz grafica */
    if (argc >= 2 && strcmp(argv[1], "--json") == 0) {
        AppConfig cfg;
        config_load(&cfg, CONFIG_FILE);

        if (argc >= 3) {
            if (strcmp(argv[2], "list-ports") == 0) {
                json_list_ports();
            } else if (strcmp(argv[2], "get-config") == 0) {
                json_get_config(&cfg);
            } else if (strcmp(argv[2], "set") == 0 && argc >= 5) {
                json_set(&cfg, argv[3], argv[4]);
            } else if (strcmp(argv[2], "monitor") == 0) {
                json_monitor(&cfg);
            } else if (strcmp(argv[2], "diagnostic") == 0) {
                json_diagnostic(&cfg);
            } else if (strcmp(argv[2], "restore") == 0) {
                json_restore(&cfg);
            }
        }
        return 0;
    }

    /* Modo consola original */
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
            case 1: ui_select_port(&cfg);      break;
            case 2: ui_select_baudrate(&cfg);   break;
            case 3: ui_select_board(&cfg);      break;
            case 4:
                ui_select_bar_type(&cfg);
                sensor_bar_init(&bar, cfg.bar_type);
                bar.threshold_black = cfg.threshold_black;
                bar.threshold_white = cfg.threshold_white;
                bar.timeout_ms      = cfg.timeout_ms;
                break;
            case 5: ui_configure_thresholds(&cfg, &bar); break;
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
            case 0: running = 0; break;
            default: break;
        }
    }

    serial_close(&sp);
    return 0;
}
