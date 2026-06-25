#ifndef UPLOAD_H
#define UPLOAD_H

int upload_backup_firmware(const char *avrdude, const char *port,
                           const char *fqbn, const char *out_hex);

int upload_restore_firmware(const char *avrdude, const char *port,
                            const char *fqbn, const char *hex_path);

int upload_patch_baudrate(const char *src_ino, const char *dst_ino, int baudrate);

int upload_flash_sketch(const char *arduino_cli, const char *sketch_dir,
                        const char *fqbn, const char *port);

#endif
