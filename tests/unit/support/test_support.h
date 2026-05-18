#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#endif

#include "sim_defs.h"

typedef void (*simh_test_stream_writer)(void *context);

const char *simh_test_source_root(void);
const char *simh_test_binary_root(void);
void simh_test_init_device_unit(DEVICE *device, UNIT *unit,
                                const char *dev_name, const char *unit_name,
                                uint32_t dev_flags, uint32_t unit_flags,
                                uint32_t dwidth, uint32_t aincr);
int simh_test_join_path(char *buffer, size_t buffer_size, const char *base,
                        const char *relative_path);
int simh_test_fixture_path(char *buffer, size_t buffer_size,
                           const char *relative_path);
int simh_test_make_temp_dir(char *buffer, size_t buffer_size,
                            const char *prefix);
int simh_test_read_file(const char *path, void **data_out, size_t *size_out);
int simh_test_read_fixture(const char *relative_path, void **data_out,
                           size_t *size_out);
int simh_test_write_file(const char *path, const void *data, size_t size);
int simh_test_read_stream(FILE *stream, char **text_out, size_t *size_out);
int simh_test_capture_stdout(simh_test_stream_writer writer, void *context,
                             char **text_out, size_t *size_out);
int simh_test_files_equal(const char *left_path, const char *right_path);
int simh_test_remove_path(const char *path);

#endif
