/* sim_disk_ramdisk.c: private ramdisk support for sim_disk */
// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: MIT

#include "sim_disk_ramdisk.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scp.h"
#include "sim_fio.h"

#define RAMDISK_PREFIX "RAMDISK:"
#define RAMDISK_PREFIX_LEN (sizeof(RAMDISK_PREFIX) - 1)

struct sim_disk_ramdisk {
    t_offset size;
    uint8_t *buffer;
    char *save_path;
    bool save_null;
};

#if defined(HAVE_FMEMOPEN)
struct sim_disk_ramdisk_spec {
    bool has_size;
    t_offset size;
    bool has_type;
    char type[CBUFSIZE];
    bool has_from;
    char from[CBUFSIZE];
    bool has_save;
    char save[CBUFSIZE];
};
#endif

/* Return true when an attach argument names an in-memory RAMDISK:. */
bool sim_disk_ramdisk_is_spec(const char *spec)
{
    return (spec != NULL) &&
           (strncasecmp(spec, RAMDISK_PREFIX, RAMDISK_PREFIX_LEN) == 0);
}

/* Reject attach switches that only make sense for disk container files. */
t_stat sim_disk_ramdisk_reject_container_switches(UNIT *uptr)
{
    static const char rejected_switches[] = {
        'C', 'D', 'E', 'I', 'K', 'M', 'O', 'V', 'X',
    };

    for (size_t index = 0;
         index < sizeof(rejected_switches) / sizeof(rejected_switches[0]);
         index++) {
        if (sim_switches & SWMASK(rejected_switches[index]))
            return sim_messagef(SCPE_ARG, "%s: RAMDISK: does not support -%c\n",
                                sim_uname(uptr), rejected_switches[index]);
    }
    return SCPE_OK;
}

/* Report that this build cannot provide RAMDISK: attachments. */
t_stat sim_disk_ramdisk_unavailable(UNIT *uptr)
{
    return sim_messagef(SCPE_NOFNC, "%s: RAMDISK: requires fmemopen support\n",
                        sim_uname(uptr));
}

#if defined(HAVE_FMEMOPEN)
/* Remove leading and trailing whitespace from a mutable option token. */
static char *trim_token(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while ((end > text) && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

/* Parse RAMDISK:SIZE values as byte counts with binary disk suffixes. */
static t_stat parse_size(const char *text, t_offset *size)
{
    char *end;
    uint64_t value;
    uint64_t multiplier = 1;
    uint64_t max_offset;

    if ((text == NULL) || (*text == '\0'))
        return SCPE_ARG;
    errno = 0;
    value = (uint64_t)strtoull(text, &end, 10);
    if ((end == text) || (errno == ERANGE))
        return SCPE_ARG;
    if (*end != '\0') {
        switch (toupper((unsigned char)*end)) {
        case 'B':
            multiplier = 1;
            break;
        case 'K':
            multiplier = UINT64_C(1024);
            break;
        case 'M':
            multiplier = UINT64_C(1024) * UINT64_C(1024);
            break;
        case 'G':
            multiplier = UINT64_C(1024) * UINT64_C(1024) * UINT64_C(1024);
            break;
        default:
            return SCPE_ARG;
        }
        end++;
        if ((*end == 'B') || (*end == 'b'))
            end++;
        if (*end != '\0')
            return SCPE_ARG;
    }

    if (sizeof(t_offset) < sizeof(uint64_t))
        max_offset = (UINT64_C(1) << ((sizeof(t_offset) * CHAR_BIT) - 1)) - 1;
    else
        max_offset = UINT64_MAX >> 1;
    if ((value == 0) || (value > (max_offset / multiplier)))
        return SCPE_ARG;
    *size = (t_offset)(value * multiplier);
    return SCPE_OK;
}

/* Set the parsed ramdisk size, rejecting duplicate or malformed sizes. */
static t_stat set_size(sim_disk_ramdisk_spec *spec, const char *text)
{
    t_offset size;

    if (spec->has_size)
        return SCPE_ARG;
    if (parse_size(text, &size) != SCPE_OK)
        return SCPE_ARG;
    spec->has_size = true;
    spec->size = size;
    return SCPE_OK;
}

/* Parse the RAMDISK: attach argument into validated option fields. */
t_stat sim_disk_ramdisk_parse_spec(const char *spec,
                                   sim_disk_ramdisk_spec **parsed)
{
    sim_disk_ramdisk_spec *options;
    char option_text[CBUFSIZE];
    char *cursor;
    char *token;

    options = calloc(1, sizeof(*options));
    if (options == NULL)
        return SCPE_MEM;
    if (strlen(spec + RAMDISK_PREFIX_LEN) >= sizeof(option_text)) {
        free(options);
        return SCPE_ARG;
    }
    strlcpy(option_text, spec + RAMDISK_PREFIX_LEN, sizeof(option_text));
    if (option_text[0] == '\0') {
        *parsed = options;
        return SCPE_OK;
    }

    cursor = option_text;
    while (cursor != NULL) {
        char *key;
        char *value;
        char *equals;
        char *next;

        next = strchr(cursor, ',');
        if (next != NULL) {
            *next = '\0';
            next++;
        }
        token = cursor;
        cursor = next;
        token = trim_token(token);
        if (*token == '\0')
            goto invalid;
        equals = strchr(token, '=');
        if (equals == NULL) {
            if (set_size(options, token) != SCPE_OK)
                goto invalid;
            continue;
        }

        *equals = '\0';
        key = trim_token(token);
        value = trim_token(equals + 1);
        if (*value == '\0')
            goto invalid;
        if (strcasecmp(key, "SIZE") == 0) {
            if (set_size(options, value) != SCPE_OK)
                goto invalid;
        } else if (strcasecmp(key, "TYPE") == 0) {
            if (options->has_type)
                goto invalid;
            strlcpy(options->type, value, sizeof(options->type));
            options->has_type = true;
        } else if (strcasecmp(key, "FROM") == 0) {
            if (options->has_from)
                goto invalid;
            strlcpy(options->from, value, sizeof(options->from));
            options->has_from = true;
        } else if (strcasecmp(key, "SAVE") == 0) {
            if (options->has_save)
                goto invalid;
            strlcpy(options->save, value, sizeof(options->save));
            options->has_save = true;
        } else {
            goto invalid;
        }
    }

    *parsed = options;
    return SCPE_OK;

invalid:
    free(options);
    return SCPE_ARG;
}

/* Release a parsed RAMDISK: attach argument. */
void sim_disk_ramdisk_free_spec(sim_disk_ramdisk_spec *spec)
{
    free(spec);
}

/* Return true when the attach argument explicitly supplies TYPE=. */
bool sim_disk_ramdisk_spec_has_type(const sim_disk_ramdisk_spec *spec)
{
    return (spec != NULL) && spec->has_type;
}

/* Return the parsed TYPE= value. */
const char *sim_disk_ramdisk_spec_type(const sim_disk_ramdisk_spec *spec)
{
    return spec->type;
}

/* Copy a flat SIMH disk image into the zero-filled ramdisk buffer. */
static t_stat seed_from_file(UNIT *uptr, sim_disk_ramdisk *ramdisk,
                             const char *path, bool exact_size)
{
    FILE *source;
    t_offset source_size;
    size_t remaining;
    uint8_t *cursor;

    source = sim_fopen(path, "rb");
    if (source == NULL)
        return sim_messagef(SCPE_OPENERR,
                            "%s: Cannot open RAMDISK: source '%s': %s\n",
                            sim_uname(uptr), path, strerror(errno));
    source_size = sim_fsize_ex(source);
    if (source_size < 0) {
        fclose(source);
        return sim_messagef(SCPE_ARG,
                            "%s: Cannot determine RAMDISK: source size "
                            "'%s'\n",
                            sim_uname(uptr), path);
    }
    if (exact_size && (source_size != ramdisk->size)) {
        fclose(source);
        return sim_messagef(SCPE_ARG,
                            "%s: RAMDISK: restore image '%s' does not "
                            "match the ramdisk size\n",
                            sim_uname(uptr), path);
    }
    if (!exact_size && (source_size > ramdisk->size)) {
        fclose(source);
        return sim_messagef(SCPE_ARG,
                            "%s: RAMDISK: source '%s' is larger than "
                            "the ramdisk\n",
                            sim_uname(uptr), path);
    }

    remaining = (size_t)source_size;
    cursor = ramdisk->buffer;
    while (remaining > 0) {
        size_t chunk = sim_fread(cursor, 1, remaining, source);

        if (chunk == 0) {
            fclose(source);
            return sim_messagef(SCPE_IOERR,
                                "%s: Error reading RAMDISK: source '%s'\n",
                                sim_uname(uptr), path);
        }
        cursor += chunk;
        remaining -= chunk;
    }
    fclose(source);
    return SCPE_OK;
}

/* Create the memory buffer and FILE stream for a RAMDISK: attachment. */
t_stat sim_disk_ramdisk_create(UNIT *uptr, const sim_disk_ramdisk_spec *spec,
                               t_offset default_size, uint32_t sector_size,
                               bool restoring, const char *mode, FILE **fileref,
                               sim_disk_ramdisk **ramdisk)
{
    sim_disk_ramdisk *created;
    t_stat r;

    created = calloc(1, sizeof(*created));
    if (created == NULL)
        return SCPE_MEM;
    created->size = spec->has_size ? spec->size : default_size;
    if ((created->size == 0) ||
        ((created->size % (t_offset)sector_size) != 0)) {
        sim_disk_ramdisk_free(created);
        return sim_messagef(SCPE_ARG,
                            "%s: RAMDISK: size must be a nonzero multiple "
                            "of the sector size (%u bytes)\n",
                            sim_uname(uptr), sector_size);
    }
    if ((t_offset)(size_t)created->size != created->size) {
        sim_disk_ramdisk_free(created);
        return sim_messagef(SCPE_MEM,
                            "%s: RAMDISK: size is too large for this host\n",
                            sim_uname(uptr));
    }
    created->buffer = calloc(1, (size_t)created->size);
    if (created->buffer == NULL) {
        sim_disk_ramdisk_free(created);
        return SCPE_MEM;
    }
    created->save_path = strdup(spec->has_save ? spec->save : NULL_DEVICE);
    if (created->save_path == NULL) {
        sim_disk_ramdisk_free(created);
        return SCPE_MEM;
    }
    created->save_null = !spec->has_save;

    if (restoring) {
        if (!spec->has_save) {
            sim_disk_ramdisk_free(created);
            return sim_messagef(SCPE_ARG,
                                "%s: RAMDISK: cannot restore contents "
                                "without SAVE=\n",
                                sim_uname(uptr));
        }
        r = seed_from_file(uptr, created, created->save_path, true);
    } else if (spec->has_from) {
        r = seed_from_file(uptr, created, spec->from, false);
    } else {
        r = SCPE_OK;
    }
    if (r != SCPE_OK) {
        sim_disk_ramdisk_free(created);
        return r;
    }

    *fileref = fmemopen(created->buffer, (size_t)created->size, mode);
    if (*fileref == NULL) {
        sim_disk_ramdisk_free(created);
        return sim_messagef(SCPE_OPENERR, "%s: Cannot create RAMDISK: - %s\n",
                            sim_uname(uptr), strerror(errno));
    }
    if (created->save_null)
        sim_messagef(SCPE_OK,
                     "%s: RAMDISK: no SAVE= specified; simulator "
                     "SAVE/RESTORE will not preserve contents\n",
                     sim_uname(uptr));
    *ramdisk = created;
    return SCPE_OK;
}
#endif

/* Return the byte size of a live ramdisk attachment. */
t_offset sim_disk_ramdisk_size(const sim_disk_ramdisk *ramdisk)
{
    return ramdisk->size;
}

/* Persist a live ramdisk attachment to its SAVE= image. */
t_stat sim_disk_ramdisk_save(UNIT *uptr, const sim_disk_ramdisk *ramdisk)
{
    FILE *save_file;
    size_t bytes_written;
    int close_status;

    if (ramdisk == NULL)
        return SCPE_OK;
    if (fflush(uptr->fileref) != 0)
        return sim_messagef(SCPE_IOERR,
                            "%s: Cannot flush RAMDISK: contents before "
                            "SAVE\n",
                            sim_uname(uptr));
    save_file = sim_fopen(ramdisk->save_path, "wb");
    if (save_file == NULL)
        return sim_messagef(
            SCPE_OPENERR, "%s: Cannot open RAMDISK: SAVE image '%s': %s\n",
            sim_uname(uptr), ramdisk->save_path, strerror(errno));
    bytes_written =
        sim_fwrite(ramdisk->buffer, 1, (size_t)ramdisk->size, save_file);
    close_status = fclose(save_file);
    if ((bytes_written != (size_t)ramdisk->size) || (close_status == EOF))
        return sim_messagef(SCPE_IOERR,
                            "%s: Error writing RAMDISK: SAVE image '%s'\n",
                            sim_uname(uptr), ramdisk->save_path);
    return SCPE_OK;
}

/* Release the resources owned by a live ramdisk attachment. */
void sim_disk_ramdisk_free(sim_disk_ramdisk *ramdisk)
{
    if (ramdisk == NULL)
        return;
    free(ramdisk->buffer);
    free(ramdisk->save_path);
    free(ramdisk);
}
