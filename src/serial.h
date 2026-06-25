#ifndef SERIAL_H
#define SERIAL_H

#include <windows.h>

#define SERIAL_MAX_PORTS    32
#define SERIAL_PORT_NAME_LEN 16
#define SERIAL_BUF_LEN      1024

typedef struct {
    char   name[SERIAL_PORT_NAME_LEN];
    HANDLE handle;
    int    baudrate;
} SerialPort;

int  serial_detect_ports(char ports[][SERIAL_PORT_NAME_LEN], int max_count);
int  serial_open(SerialPort *sp, const char *name, int baudrate);
void serial_close(SerialPort *sp);

/* Lee una línea terminada en '\n'. Retorna: >0 bytes leídos, 0 timeout, -1 error. */
int  serial_read_line(SerialPort *sp, char *buf, int buf_len, int timeout_ms);

#endif
