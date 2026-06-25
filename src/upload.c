#include "upload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int run_command(const char *cmdline) {
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, (char *)cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

int upload_backup_firmware(const char *avrdude, const char *port,
                           const char *fqbn, const char *out_hex) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "%s -v -p m328p -c arduino -P %s -D -U flash:r:%s:i",
             avrdude, port, out_hex);

    printf("\n  Guardando firmware original desde %s...\n", port);
    printf("  Comando: %s\n", cmd);

    int ret = run_command(cmd);
    if (ret != 0) {
        printf("  Error: avrdude retorno codigo %d\n", ret);
        return 0;
    }

    printf("  Firmware guardado en: %s\n", out_hex);
    return 1;
}

int upload_restore_firmware(const char *avrdude, const char *port,
                            const char *fqbn, const char *hex_path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "%s -v -p m328p -c arduino -P %s -D -U flash:w:%s:i",
             avrdude, port, hex_path);

    printf("\n  Restaurando firmware original en %s...\n", port);
    printf("  Comando: %s\n", cmd);

    int ret = run_command(cmd);
    if (ret != 0) {
        printf("  Error: avrdude retorno codigo %d\n", ret);
        printf("  Para restaurar manualmente, ejecutar:\n");
        printf("  %s\n", cmd);
        printf("  Archivo hex disponible en: %s\n", hex_path);
        return 0;
    }

    printf("  Firmware restaurado exitosamente.\n");
    return 1;
}

int upload_patch_baudrate(const char *src_ino, const char *dst_ino, int baudrate) {
    FILE *src = fopen(src_ino, "r");
    if (!src) {
        printf("  Error: no se puede abrir %s\n", src_ino);
        return 0;
    }

    FILE *dst = fopen(dst_ino, "w");
    if (!dst) {
        printf("  Error: no se puede abrir %s para escritura\n", dst_ino);
        fclose(src);
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), src)) {
        if (strstr(line, "Serial.begin(")) {
            fprintf(dst, "  Serial.begin(%d);\n", baudrate);
        } else {
            fputs(line, dst);
        }
    }

    fclose(src);
    fclose(dst);
    return 1;
}

int upload_flash_sketch(const char *arduino_cli, const char *sketch_dir,
                        const char *fqbn, const char *port) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "%s compile --fqbn %s %s && %s upload -p %s --fqbn %s %s",
             arduino_cli, fqbn, sketch_dir,
             arduino_cli, port, fqbn, sketch_dir);

    printf("\n  Compilando y subiendo sketch...\n");
    printf("  Comando: %s\n", cmd);

    int ret = run_command(cmd);
    if (ret != 0) {
        printf("  Error: arduino-cli retorno codigo %d\n", ret);
        return 0;
    }

    printf("  Sketch compilado y subido exitosamente.\n");
    return 1;
}
