#include "ui.h"
#include "utils.h"
#include "config.h"
#include "upload.h"
#include "boards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdarg.h>

/* ── ANSI escape codes ──────────────────────────────────────────────────── */
#define A_RESET    "\033[0m"
#define A_BOLD     "\033[1m"
#define A_DIM      "\033[2m"
#define A_BBLACK   "\033[90m"
#define A_BRED     "\033[91m"
#define A_BGREEN   "\033[92m"
#define A_BYELLOW  "\033[93m"
#define A_BBLUE    "\033[94m"
#define A_BCYAN    "\033[96m"
#define A_BWHITE   "\033[97m"
#define A_WHITE    "\033[37m"

/* Semantic color aliases */
#define C_NEGRO    A_BGREEN
#define C_GRIS     A_BYELLOW
#define C_BLANCO   A_WHITE
#define C_TIMEOUT  A_BRED
#define C_CALIB    A_BCYAN
#define C_ACCENT   A_BCYAN
#define C_HEADER   A_BOLD A_BWHITE
#define C_FREEZE   A_BOLD A_BYELLOW
#define C_DIM      A_BBLACK
#define C_MARK     A_BOLD A_BGREEN

/* VT erase-to-end-of-line (for live-redraw lines) */
#define EL         "\033[K"

/* ── Box-drawing chars as UTF-8 hex literals ───────────────────────────── */
/* Double-line */
#define BH   "\xe2\x95\x90"   /* = U+2550 */
#define BV   "\xe2\x95\x91"   /* || U+2551 */
#define BTL  "\xe2\x95\x94"   /* ╔ U+2554 */
#define BTR  "\xe2\x95\x97"   /* ╗ U+2557 */
#define BBL  "\xe2\x95\x9a"   /* ╚ U+255A */
#define BBR  "\xe2\x95\x9d"   /* ╝ U+255D */
#define BML  "\xe2\x95\xa0"   /* ╠ U+2560 */
#define BMR  "\xe2\x95\xa3"   /* ╣ U+2563 */
/* Single-line thin */
#define SH   "\xe2\x94\x80"   /* ─ U+2500 */
#define SV   "\xe2\x94\x82"   /* │ U+2502 */
/* Block fill */
#define BLK  "\xe2\x96\x88"   /* █ U+2588 */
#define SHD  "\xe2\x96\x91"   /* ░ U+2591 */

/* ── One-time VT / UTF-8 initialisation ────────────────────────────────── */
static void init_console(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);   /* UTF-8 output */
    SetConsoleCP(65001);
    SetConsoleTitleA("Visor Barra de Sensores");
}

/* ── Helpers de consola ─────────────────────────────────────────────────── */

void ui_clear(void) {
    static int ready = 0;
    if (!ready) { init_console(); ready = 1; }
    system("cls");
}

static void cursor_home(void) {
    COORD c = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

/* Ajusta el ancho de la ventana y buffer de consola para n sensores */
static void console_resize_for_sensors(int n) {
    int needed = 10 + 1 + n * 7 + 4;
    if (needed < 80) needed = 80;

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi)) return;

    SMALL_RECT win = csbi.srWindow;
    SHORT cur_w    = (SHORT)(win.Right - win.Left + 1);

    if ((SHORT)needed > csbi.dwSize.X) {
        COORD buf = csbi.dwSize;
        buf.X = (SHORT)needed;
        SetConsoleScreenBufferSize(h, buf);
    }

    if ((SHORT)needed > cur_w) {
        win.Right = win.Left + (SHORT)(needed - 1);
        SetConsoleWindowInfo(h, TRUE, &win);
    }
}

static void cursor_hide(int hide) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = hide ? FALSE : TRUE;
    SetConsoleCursorInfo(h, &ci);
}

static void color_reset(void) { printf(A_RESET); }

static void color_for_status(DiagStatus s) {
    switch (s) {
        case DIAG_NEGRO:        printf(C_NEGRO);   break;
        case DIAG_GRIS:         printf(C_GRIS);    break;
        case DIAG_FAIL_TIMEOUT: printf(C_TIMEOUT); break;
        default:                printf(C_BLANCO);  break;
    }
}

/* Lee un entero por consola con fgets */
static int read_menu_int(void) {
    char buf[32] = {0};
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    return atoi(buf);
}

void ui_pause(void) {
    printf("\n" C_DIM "  Presione Enter para continuar..." A_RESET);
    fflush(stdout);
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    char buf[8];
    fgets(buf, sizeof(buf), stdin);
}

/* ── Decoradores de caja ────────────────────────────────────────────────── */

/* BOX_W = 48 chars de contenido visible entre los dos BV */
#define BOX_W 48

static void box_hline_top(void) {
    printf(C_ACCENT BTL);
    for (int i = 0; i < BOX_W; i++) printf(BH);
    printf(BTR A_RESET "\n");
}

static void box_hline_mid(void) {
    printf(C_ACCENT BML);
    for (int i = 0; i < BOX_W; i++) printf(BH);
    printf(BMR A_RESET "\n");
}

static void box_hline_bot(void) {
    printf(C_ACCENT BBL);
    for (int i = 0; i < BOX_W; i++) printf(BH);
    printf(BBR A_RESET "\n");
}

/* Fila de caja con contenido ASCII puro alineado a BOX_W */
static void box_row(const char *fmt, ...) {
    char content[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(content, sizeof(content), fmt, ap);
    va_end(ap);

    int len = (int)strlen(content);
    printf(C_ACCENT BV A_RESET " %s", content);
    for (int i = len + 1; i < BOX_W; i++) printf(" ");
    printf(" " C_ACCENT BV A_RESET "\n");
}

static void box_row_blank(void) { box_row(""); }

/* Fila con numero de opcion coloreado */
static void box_menu_item(int num, const char *text, int marked) {
    char label[4];
    snprintf(label, sizeof(label), "%d", num);

    printf(C_ACCENT BV A_RESET "  ");
    if (marked) printf(C_MARK); else printf(C_DIM);
    printf("%s" A_RESET "  ", label);
    if (marked) printf(C_MARK);
    int len = (int)strlen(text);
    printf("%s" A_RESET, text);
    /* pad to BOX_W: 1(space after BV) + 2(indent) + 1(num) + 2(spaces) + len + pad + 1(space before BV) */
    int used = 1 + 2 + (int)strlen(label) + 2 + len + 1;
    for (int i = used; i < BOX_W; i++) printf(" ");
    printf(C_ACCENT BV A_RESET "\n");
}

/* Thin separator line (no box sides) */
static void thin_sep(int width) {
    printf("  " C_DIM);
    for (int i = 0; i < width; i++) printf(SH);
    printf(A_RESET "\n");
}

/* ── Cabecera estandar ──────────────────────────────────────────────────── */

static void ui_header(const char *title) {
    /* Inner: "  VISOR CONSOLA  |  " (20) + title (%-26s = 26) + "  " (2) = 48 */
    box_hline_top();
    printf(C_ACCENT BV A_RESET
           "  " C_HEADER "VISOR CONSOLA" A_RESET
           "  " C_DIM SV A_RESET
           "  %-26s"
           C_ACCENT "  " BV A_RESET "\n", title);
    box_hline_bot();
    printf("\n");
}

/* ── Menu principal ─────────────────────────────────────────────────────── */

int ui_main_menu(const AppConfig *cfg) {
    ui_clear();
    ui_header("MENU PRINCIPAL");

    box_hline_top();
    box_row(" Configuracion actual");
    box_hline_mid();
    box_row("  %-10s " SV "  %s", "Barra",
            SENSOR_BAR_MODELS[cfg->bar_type].name);
    box_row("  %-10s " SV "  %s", "Puerto",
            cfg->port[0] ? cfg->port : "(sin seleccionar)");

    char baud_s[16];
    snprintf(baud_s, sizeof(baud_s), "%d baud", cfg->baudrate);
    box_row("  %-10s " SV "  %s", "Baudrate", baud_s);
    box_hline_mid();
    box_row_blank();
    box_menu_item(1, "Seleccionar puerto",          0);
    box_menu_item(2, "Seleccionar baudrate",         0);
    box_menu_item(3, "Seleccionar placa (board)",    0);
    box_menu_item(4, "Tipo de barra de sensores",    0);
    box_menu_item(5, "Configurar umbrales",          0);
    box_menu_item(6, "Iniciar lectura / diagnostico",1);
    box_menu_item(7, "Guardar configuracion",        0);
    box_menu_item(0, "Salir",                        0);
    box_row_blank();
    box_hline_bot();

    printf("\n" C_ACCENT "  Opcion: " A_RESET);
    fflush(stdout);

    return read_menu_int();
}

/* ── Seleccion de puerto ────────────────────────────────────────────────── */

void ui_select_port(AppConfig *cfg) {
    ui_clear();
    ui_header("SELECCIONAR PUERTO");

    char ports[SERIAL_MAX_PORTS][SERIAL_PORT_NAME_LEN];
    int  count = serial_detect_ports(ports, SERIAL_MAX_PORTS);

    if (count == 0) {
        printf(C_TIMEOUT "  No se detectaron puertos COM disponibles.\n" A_RESET);
        printf("\n");
        thin_sep(44);
        printf(C_DIM "  0  Volver\n" A_RESET);
        thin_sep(44);
        printf("\n" C_ACCENT "  Opcion: " A_RESET); fflush(stdout);
        read_menu_int();
        return;
    }

    thin_sep(44);
    printf(C_DIM "  0  Volver\n" A_RESET);
    for (int i = 0; i < count; i++) {
        int sel = (strcmp(ports[i], cfg->port) == 0);
        if (sel) printf(C_MARK); else printf("  ");
        printf("%d  %s", i + 1, ports[i]);
        if (sel) printf("  " C_DIM "<-- activo" A_RESET);
        printf("\n");
    }
    thin_sep(44);

    printf("\n" C_ACCENT "  Seleccione (0-%d): " A_RESET, count);
    fflush(stdout);

    int sel = read_menu_int();
    if (sel >= 1 && sel <= count)
        strncpy(cfg->port, ports[sel - 1], CONFIG_PORT_LEN - 1);
}

/* ── Seleccion de baudrate ──────────────────────────────────────────────── */

void ui_select_baudrate(AppConfig *cfg) {
    static const int RATES[] = {9600, 19200, 38400, 57600, 115200};
    static const int NRATES  = 5;

    ui_clear();
    ui_header("SELECCIONAR BAUDRATE");

    thin_sep(44);
    printf(C_DIM "  0  Volver\n" A_RESET);
    for (int i = 0; i < NRATES; i++) {
        int sel = (RATES[i] == cfg->baudrate);
        if (sel) printf(C_MARK); else printf("  ");
        printf("%d  %6d baud", i + 1, RATES[i]);
        if (sel) printf("  " C_DIM "<-- activo" A_RESET);
        printf("\n");
    }
    thin_sep(44);

    printf("\n" C_ACCENT "  Seleccione (0-%d): " A_RESET, NRATES);
    fflush(stdout);

    int sel = read_menu_int();
    if (sel >= 1 && sel <= NRATES)
        cfg->baudrate = RATES[sel - 1];
}

/* ── Seleccion de placa (board) ─────────────────────────────────────────── */

void ui_select_board(AppConfig *cfg) {
    ui_clear();
    ui_header("SELECCIONAR PLACA (BOARD)");

    thin_sep(44);
    printf(C_DIM "  0  Volver\n" A_RESET);
    for (int i = 0; i < KNOWN_BOARD_COUNT; i++) {
        int sel = (strcmp(KNOWN_BOARDS[i].fqbn, cfg->board_fqbn) == 0);
        if (sel) printf(C_MARK); else printf("  ");
        printf("%d  %-28s", i + 1, KNOWN_BOARDS[i].name);
        if (sel) printf("  " C_DIM "<-- activo" A_RESET);
        printf("\n");
    }
    printf("\n" C_DIM "  %d  Ingresar FQBN manual" A_RESET "\n", KNOWN_BOARD_COUNT + 1);
    thin_sep(44);

    printf("\n" C_ACCENT "  Seleccione (0-%d): " A_RESET, KNOWN_BOARD_COUNT + 1);
    fflush(stdout);

    int sel = read_menu_int();
    if (sel >= 1 && sel <= KNOWN_BOARD_COUNT) {
        strncpy(cfg->board_fqbn, KNOWN_BOARDS[sel - 1].fqbn, sizeof(cfg->board_fqbn) - 1);
    } else if (sel == KNOWN_BOARD_COUNT + 1) {
        printf("\n" C_ACCENT "  FQBN (ej: arduino:avr:nano:cpu=atmega328old): " A_RESET);
        fflush(stdout);
        char buf[256];
        if (fgets(buf, sizeof(buf), stdin)) {
            buf[strcspn(buf, "\n\r")] = '\0';
            if (strlen(buf) > 0) {
                strncpy(cfg->board_fqbn, buf, sizeof(cfg->board_fqbn) - 1);
            }
        }
    }
}

/* ── Seleccion de tipo de barra ─────────────────────────────────────────── */

void ui_select_bar_type(AppConfig *cfg) {
    ui_clear();
    ui_header("TIPO DE BARRA DE SENSORES");

    thin_sep(44);
    printf(C_DIM "  0  Volver\n" A_RESET);
    for (int i = 0; i < BAR_COUNT; i++) {
        int sel = (i == (int)cfg->bar_type);
        if (sel) printf(C_MARK); else printf("  ");
        printf("%d  %-14s (%d sensores)",
               i + 1,
               SENSOR_BAR_MODELS[i].name,
               SENSOR_BAR_MODELS[i].sensor_count);
        if (sel) printf("  " C_DIM "<-- activo" A_RESET);
        printf("\n");
    }
    thin_sep(44);

    printf("\n" C_ACCENT "  Seleccione (0-%d): " A_RESET, BAR_COUNT);
    fflush(stdout);

    int sel = read_menu_int();

    if (sel >= 1 && sel <= BAR_COUNT) {
        cfg->bar_type = (BarType)(sel - 1);
        const SensorBarModel *m = &SENSOR_BAR_MODELS[cfg->bar_type];
        cfg->baudrate        = m->default_baudrate;
        cfg->threshold_black = m->default_threshold_black;
        cfg->threshold_white = m->default_threshold_white;
        cfg->timeout_ms      = m->default_timeout_ms;

        if (!config_save(cfg, CONFIG_FILE)) {
            printf("\n" C_TIMEOUT
                   "  Advertencia: no se pudo guardar la configuracion en %s.\n"
                   "  Los cambios se aplicaron en esta sesion pero no persisten.\n"
                   A_RESET, CONFIG_FILE);
            ui_pause();
        }
    }
}

/* ── Configurar umbrales ────────────────────────────────────────────────── */

void ui_configure_thresholds(AppConfig *cfg, SensorBar *bar) {
    ui_clear();
    ui_header("CONFIGURAR UMBRALES");

    thin_sep(44);
    printf(C_DIM "  0  Volver sin cambios\n" A_RESET);
    printf("  1  Editar umbrales\n");
    thin_sep(44);
    printf("\n" C_ACCENT "  Opcion: " A_RESET);
    fflush(stdout);

    int opt = read_menu_int();
    if (opt != 1) return;

    ui_clear();
    ui_header("CONFIGURAR UMBRALES");
    printf(C_DIM "  Rango calibrado: 0 " A_RESET C_BLANCO "(blanco)" A_RESET
           C_DIM " — 1000 " A_RESET C_NEGRO "(negro)" A_RESET "\n");
    printf(C_DIM "  Enter sin valor mantiene el actual.\n\n" A_RESET);

    char buf[32];
    int  tmp;

    printf("  Umbral " C_NEGRO "NEGRO" A_RESET "  >= [" C_MARK "%d" A_RESET "]: ",
           cfg->threshold_black);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (tmp = atoi(buf)) > 0)
        cfg->threshold_black = tmp;

    printf("  Umbral " C_BLANCO "BLANCO" A_RESET " <  [" C_MARK "%d" A_RESET "]: ",
           cfg->threshold_white);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (tmp = atoi(buf)) > 0)
        cfg->threshold_white = tmp;

    printf("  Timeout (ms)        [" C_MARK "%d" A_RESET "]: ",
           cfg->timeout_ms);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (tmp = atoi(buf)) > 0)
        cfg->timeout_ms = tmp;

    bar->threshold_black = cfg->threshold_black;
    bar->threshold_white = cfg->threshold_white;
    bar->timeout_ms      = cfg->timeout_ms;
}

/* ── Celdas de la pantalla de lectura ───────────────────────────────────── */

/*
 * Columnas: etiqueta 10 chars + '|', cada sensor 7 chars + '|'
 *   Valor : " %5d |"  = 7
 *   Barra : "[XXXX]|"  = 7  (4 bloques Unicode = 4 chars visuales)
 *   Estado: "%-6s |"  = 7
 */

static void draw_bar_cell(int value, DiagStatus s) {
    int filled = value * 4 / 1000;
    if (filled < 0) filled = 0;
    if (filled > 4) filled = 4;

    printf("[");
    color_for_status(s);
    for (int j = 0; j < 4; j++)
        printf("%s", j < filled ? BLK : SHD);
    color_reset();
    printf("]|");
}

static void draw_state_cell(DiagStatus s) {
    color_for_status(s);
    printf("%-6s", diagnostic_status_str(s));
    color_reset();
    printf("|");
}

static void draw_value_cell(int value, DiagStatus s) {
    color_for_status(s);
    printf(" %4d ", value);
    color_reset();
    printf("|");
}

static void draw_calib_cell(int value) {
    printf(C_CALIB " %4d " A_RESET "|", value);
}

static void draw_empty_cell(void) {
    printf(C_DIM "  --- " A_RESET "|");
}

/* ── Pantalla de lectura en tiempo real ─────────────────────────────────── */

void ui_reading_screen(SerialPort *sp, SensorBar *bar, AppConfig *cfg) {
    char       msg_buf[SERIAL_BUF_LEN];
    memset(msg_buf, 0, sizeof(msg_buf));
    DiagReport report;
    memset(&report, 0, sizeof(report));

    bar->has_prev        = 0;
    bar->position        = 0;
    bar->has_calibration = 0;
    memset(bar->values,      0, sizeof(bar->values));
    memset(bar->prev_values, 0, sizeof(bar->prev_values));
    memset(bar->calib_min,   0, sizeof(bar->calib_min));
    memset(bar->calib_max,   0, sizeof(bar->calib_max));

    ui_clear();
    printf(C_HEADER "PREPARANDO DIAGNOSTICO\n" A_RESET "\n");

    printf("  [1/5] Guardando firmware original...\n");
    fflush(stdout);
    if (!upload_backup_firmware(cfg->avrdude_path, cfg->port, cfg->board_fqbn, cfg->backup_hex_path)) {
        printf(C_TIMEOUT "\n  Advertencia: no se pudo guardar el firmware.\n" A_RESET);
        printf("  Puede continuar pero la restauracion automatica no sera posible.\n\n");
        printf("  " C_ACCENT "Continuar? (s/n): " A_RESET);
        fflush(stdout);
        char buf[8];
        if (!fgets(buf, sizeof(buf), stdin) || (buf[0] != 's' && buf[0] != 'S')) {
            return;
        }
    }

    const char *sketch_dir = NULL;
    switch (bar->model->type) {
        case BAR_QTR8RC:
            sketch_dir = "arduino/qtr8rc_visor";
            break;
        case BAR_IM_HEM_005:
            sketch_dir = "arduino/im_hem_005";
            break;
        default:
            printf(C_TIMEOUT "  Error: tipo de barra desconocido.\n" A_RESET);
            return;
    }

    printf("  [2/5] Ajustando baudrate del sketch...\n");
    fflush(stdout);
    char src_ino[512], dst_ino[512];
    snprintf(src_ino, sizeof(src_ino), "%s/%s.ino", sketch_dir,
             (bar->model->type == BAR_QTR8RC) ? "qtr8rc_visor" : "im_hem_005");
    snprintf(dst_ino, sizeof(dst_ino), "%s_patched.ino", sketch_dir);
    if (!upload_patch_baudrate(src_ino, dst_ino, cfg->baudrate)) {
        printf(C_TIMEOUT "  Error: no se pudo ajustar el baudrate.\n" A_RESET);
        return;
    }

    printf("  [3/5] Compilando y subiendo sketch...\n");
    fflush(stdout);
    if (!upload_flash_sketch(cfg->arduino_cli_path, sketch_dir, cfg->board_fqbn, cfg->port)) {
        printf(C_TIMEOUT "  Error: fallo la compilacion o carga del sketch.\n" A_RESET);
        return;
    }

    printf("  [4/5] Esperando que el Arduino se reinicie...\n");
    fflush(stdout);
    Sleep(2000);

    printf("  [5/5] Iniciando diagnostico.\n");
    fflush(stdout);
    Sleep(1000);

    uint64_t last_rx        = utils_millis();
    int      running        = 1;
    int      timed_out      = 0;
    int      has_valid_data = 0;
    int      frozen         = 0;

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  old_mode;
    GetConsoleMode(hin, &old_mode);
    SetConsoleMode(hin, 0);

    console_resize_for_sensors(bar->model->sensor_count);
    cursor_hide(1);
    ui_clear();

    /* Pantalla de espera inicial */
    printf(C_ACCENT BTL); for(int i=0;i<44;i++) printf(BH); printf(BTR A_RESET "\n");
    printf(C_ACCENT BV A_RESET "  " C_HEADER "ESPERANDO DATOS DEL ARDUINO" A_RESET
           "%16s" C_ACCENT BV A_RESET "\n", "");
    printf(C_ACCENT BML); for(int i=0;i<44;i++) printf(BH); printf(BMR A_RESET "\n");
    printf(C_ACCENT BV A_RESET "  " C_DIM "Por favor espere ~10 segundos mientras" A_RESET
           "%6s" C_ACCENT BV A_RESET "\n", "");
    printf(C_ACCENT BV A_RESET "  " C_DIM "el Arduino calibra los sensores." A_RESET
           "%12s" C_ACCENT BV A_RESET "\n", "");
    printf(C_ACCENT BBL); for(int i=0;i<44;i++) printf(BH); printf(BBR A_RESET "\n");
    fflush(stdout);
    Sleep(2000);

    while (running) {
        /* Teclas: Q=salir, F=freeze */
        INPUT_RECORD ir;
        DWORD nevents = 0;
        while (PeekConsoleInput(hin, &ir, 1, &nevents) && nevents > 0) {
            ReadConsoleInput(hin, &ir, 1, &nevents);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                char c = ir.Event.KeyEvent.uChar.AsciiChar;
                if (c == 'q' || c == 'Q') running = 0;
                if (c == 'f' || c == 'F') frozen  = !frozen;
            }
        }
        if (!running) break;

        int ret = serial_read_line(sp, msg_buf, sizeof(msg_buf), 500);

        if (ret > 0) {
            if (!frozen) {
                if (sensor_bar_parse_message(bar, msg_buf)) {
                    last_rx        = utils_millis();
                    timed_out      = 0;
                    has_valid_data = 1;
                    diagnostic_run(bar, &report);

                    cursor_home();
                    printf(C_DIM "%-10s" A_RESET "| ", "Valores");
                    for (int i = 0; i < bar->model->sensor_count; i++) {
                        if (bar->model->type == BAR_IM_HEM_005)
                            printf("S%02d-" C_CALIB "%d" A_RESET, i, bar->values[i]);
                        else
                            printf("S%d-" C_CALIB "%d" A_RESET, i + 1, bar->values[i]);
                        if (i < bar->model->sensor_count - 1) printf(", ");
                    }
                    printf(EL "\n\n");
                    fflush(stdout);
                } else {
                    sensor_bar_parse_calibration(bar, msg_buf);
                }
            }
        }

        if (!frozen && has_valid_data && !timed_out &&
            (int)(utils_millis() - last_rx) >= bar->timeout_ms)
        {
            timed_out = 1;
            diagnostic_timeout(&report, bar->model->sensor_count);
        }

        /* ── Dibujar ──────────────────────────────────────────────────── */
        int n = bar->model->sensor_count;
        cursor_home();

        /* Header con estado de freeze */
        if (frozen) {
            printf(C_FREEZE " %s " A_RESET C_DIM SV A_RESET
                   " %-6s " C_DIM SV A_RESET " %d baud "
                   C_DIM SV A_RESET " [Q] Salir  [F] Reanudar "
                   C_FREEZE "[ CONGELADO ]" A_RESET EL "\n\n",
                   bar->model->name, sp->name, sp->baudrate);
        } else {
            printf(C_HEADER " %s " A_RESET C_DIM SV A_RESET
                   " %-6s " C_DIM SV A_RESET " %d baud "
                   C_DIM SV A_RESET " [Q] Salir  [F] Congelar" A_RESET EL "\n\n",
                   bar->model->name, sp->name, sp->baudrate);
        }

        if (!has_valid_data) {
            if (bar->has_calibration) {
                printf(C_CALIB "  Calibracion recibida. Esperando primera lectura..." A_RESET EL "\n");
            } else {
                printf(C_DIM   "  Calibrando sensores..." A_RESET EL "\n");
                printf(C_DIM   "  (El Arduino calibra ~10s antes de enviar datos)" A_RESET EL "\n");
            }
            for (int i = 0; i < 10; i++) printf(EL "\n");
        } else {
            /* ── Cabecera de columnas ─────────────────────────────────── */
            printf(C_DIM "%-10s" A_RESET "|", "Sensor");
            for (int i = 0; i < n; i++) {
                if (bar->model->type == BAR_IM_HEM_005)
                    printf(C_DIM "  S%02d  " A_RESET "|", i);
                else
                    printf(C_DIM "  S%-2d   " A_RESET "|", i + 1);
            }
            printf("\n");

            printf(C_DIM "----------|");
            for (int i = 0; i < n; i++) printf("-------|");
            printf(A_RESET "\n");

            /* ── Calibracion Min / Max ───────────────────────────────── */
            printf(C_DIM "%-10s" A_RESET "|", "Cal.Min");
            for (int i = 0; i < n; i++) {
                if (bar->has_calibration) draw_calib_cell(bar->calib_min[i]);
                else                      draw_empty_cell();
            }
            printf("\n");

            printf(C_DIM "%-10s" A_RESET "|", "Cal.Max");
            for (int i = 0; i < n; i++) {
                if (bar->has_calibration) draw_calib_cell(bar->calib_max[i]);
                else                      draw_empty_cell();
            }
            printf("\n");

            printf(C_DIM "----------|");
            for (int i = 0; i < n; i++) printf("-------|");
            printf(A_RESET "\n");

            /* ── Valor en vivo ───────────────────────────────────────── */
            printf(C_DIM "%-10s" A_RESET "|", "Valor");
            for (int i = 0; i < n; i++)
                draw_value_cell(bar->values[i], report.status[i]);
            printf("\n");

            /* ── Barra visual con bloques ────────────────────────────── */
            printf(C_DIM "%-10s" A_RESET "|", "Barra");
            for (int i = 0; i < n; i++)
                draw_bar_cell(bar->values[i], report.status[i]);
            printf("\n");

            /* ── Estado ──────────────────────────────────────────────── */
            printf(C_DIM "%-10s" A_RESET "|", "Estado");
            for (int i = 0; i < n; i++)
                draw_state_cell(report.status[i]);
            printf("\n\n");

            /* ── Posicion y resumen ───────────────────────────────────── */
            printf(C_DIM "Posicion  : " A_RESET C_HEADER "%+6d" A_RESET
                   C_DIM "  [%+d .. %+d]" A_RESET EL "\n",
                   bar->position,
                   bar->model->position_min,
                   bar->model->position_max);

            if (timed_out) {
                printf(C_TIMEOUT "  SIN DATOS  " A_RESET
                       C_DIM "(timeout %d ms)" A_RESET EL "\n", bar->timeout_ms);
            } else {
                printf("  ");
                printf(C_NEGRO  BLK " Negro:%-2d  "  A_RESET, report.negro_count);
                printf(C_GRIS   BLK " Gris:%-2d   " A_RESET, report.gris_count);
                printf(C_BLANCO BLK " Blanco:%-2d" A_RESET, report.blanco_count);
                printf(EL "\n");
            }
        }
        printf("\n");
    }

    printf("\n\n" C_HEADER "FINALIZANDO DIAGNOSTICO\n" A_RESET "\n");
    printf("  Restaurando firmware original...\n");
    fflush(stdout);
    if (!upload_restore_firmware(cfg->avrdude_path, cfg->port, cfg->board_fqbn, cfg->backup_hex_path)) {
        printf(C_TIMEOUT "\n  Advertencia: no se pudo restaurar el firmware automaticamente.\n" A_RESET);
        printf("  Para restaurar manualmente, ejecutar:\n");
        printf("  avrdude -v -p m328p -c arduino -P %s -D -U flash:w:%s:i\n",
               cfg->port, cfg->backup_hex_path);
        ui_pause();
    }

    cursor_hide(0);
    SetConsoleMode(hin, old_mode);
    ui_clear();
}
