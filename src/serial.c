#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int serial_detect_ports(char ports[][SERIAL_PORT_NAME_LEN], int max_count) {
    char device_names[65536];
    int  count = 0;

    if (!QueryDosDeviceA(NULL, device_names, sizeof(device_names)))
        return 0;

    char *p = device_names;
    while (*p && count < max_count) {
        if (strncmp(p, "COM", 3) == 0 && isdigit((unsigned char)p[3])) {
            strncpy(ports[count], p, SERIAL_PORT_NAME_LEN - 1);
            ports[count][SERIAL_PORT_NAME_LEN - 1] = '\0';
            count++;
        }
        p += strlen(p) + 1;
    }
    return count;
}

int serial_open(SerialPort *sp, const char *name, int baudrate) {
    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\%s", name);

    sp->handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (sp->handle == INVALID_HANDLE_VALUE)
        return 0;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(sp->handle, &dcb)) {
        CloseHandle(sp->handle);
        sp->handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    dcb.BaudRate = (DWORD)baudrate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(sp->handle, &dcb)) {
        CloseHandle(sp->handle);
        sp->handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    /* ReadFile espera hasta 100ms por cada byte */
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout        = 100;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant   = 0;
    SetCommTimeouts(sp->handle, &to);

    strncpy(sp->name, name, SERIAL_PORT_NAME_LEN - 1);
    sp->name[SERIAL_PORT_NAME_LEN - 1] = '\0';
    sp->baudrate = baudrate;
    return 1;
}

void serial_close(SerialPort *sp) {
    if (sp->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(sp->handle);
        sp->handle = INVALID_HANDLE_VALUE;
    }
}

int serial_read_line(SerialPort *sp, char *buf, int buf_len, int timeout_ms) {
    if (sp->handle == INVALID_HANDLE_VALUE) return -1;

    DWORD start = GetTickCount();
    int   pos   = 0;

    while (pos < buf_len - 1) {
        if ((int)(GetTickCount() - start) >= timeout_ms) {
            return 0;
        }

        char  c;
        DWORD bytes_read = 0;
        if (!ReadFile(sp->handle, &c, 1, &bytes_read, NULL)) {
            return -1;
        }

        if (bytes_read == 0)
            continue;

        if (c == '\n') {
            if (pos > 0 && buf[pos - 1] == '\r') pos--;
            buf[pos] = '\0';
            return pos;
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return pos;
}
