/* sim_tempfile.h: simulator temporary file helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_TEMPFILE_H_
#define SIM_TEMPFILE_H_ 0

#include <stddef.h>
#include <stdio.h>

/*
 * Create a uniquely named temporary file in the host temporary directory.
 *
 * The returned descriptor is opened for binary read/write access and is
 * owned by the caller.  The full path is written to path; callers must close
 * the descriptor and remove path when the file is no longer needed.
 *
 * prefix and suffix may be NULL, which is treated the same as an empty
 * string.  suffix is preserved at the end of the generated file name so
 * callers can request a recognizable extension without building
 * platform-specific temporary-file templates.
 *
 * The file is created atomically and existing files are never overwritten.
 * Returns a non-negative file descriptor on success, or -1 on failure with
 * errno set.  Invalid arguments fail with EINVAL, and too-small path buffers
 * fail with ENAMETOOLONG.
 */
int sim_tempfile_open(char *path, size_t path_size, const char *prefix,
                      const char *suffix);

/*
 * Create a uniquely named temporary file and return it as a stdio stream.
 *
 * mode is passed to fdopen after atomic file creation.  On fdopen failure,
 * the descriptor is closed, path is removed, and errno is preserved from
 * fdopen.  The caller owns the returned stream and path.
 */
FILE *sim_tempfile_open_stream(char *path, size_t path_size, const char *prefix,
                               const char *suffix, const char *mode);

/*
 * Create a temporary binary read/write stream with no pathname contract.
 *
 * The caller owns the returned stream and must close it with fclose.  The
 * backing file is removed automatically when the stream is closed.
 */
FILE *sim_tmpfile(void);

#endif
