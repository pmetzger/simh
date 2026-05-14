/* sim_host_path.h: Host path-name discovery helpers

   Declare reusable helpers that choose host path spellings whose values
   vary by operating system, such as the temporary-file directory.
*/
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_HOST_PATH_H_
#define SIM_HOST_PATH_H_ 1

#include <stddef.h>

/* Return the host temporary-file directory path in caller-provided storage. */
const char *sim_host_temp_dir(char *buf, size_t buf_size);

#endif
