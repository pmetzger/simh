#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "altairz80_defs.h"

static size_t fw2_test_last_malloc_size;

static void *fw2_test_malloc(size_t size)
{
    fw2_test_last_malloc_size = size;
    return malloc(size);
}

static uint32_t fw2_test_sim_map_resource(
    uint32_t baseaddr, uint32_t size, uint32_t resource_type,
    int32_t (*routine)(const int32_t, const int32_t, const int32_t),
    const char *name, uint8_t unmap)
{
    (void)baseaddr;
    (void)size;
    (void)resource_type;
    (void)routine;
    (void)name;
    (void)unmap;

    return SCPE_OK;
}

int32_t sio0s(const int32_t port, const int32_t io, const int32_t data);
int32_t sio0d(const int32_t port, const int32_t io, const int32_t data);
int32_t getBankSelect(void);

int32_t sio0s(const int32_t port, const int32_t io, const int32_t data)
{
    (void)port;
    (void)io;
    (void)data;

    return 0;
}

int32_t sio0d(const int32_t port, const int32_t io, const int32_t data)
{
    (void)port;
    (void)io;
    (void)data;

    return 0;
}

int32_t getBankSelect(void)
{
    return 0;
}

#define malloc fw2_test_malloc
#define sim_map_resource fw2_test_sim_map_resource
/*
 * This include-based test preserves coverage for a real attach-path
 * allocation bug. Replace it with a cleaner higher-boundary behavior
 * test when Flashwriter II attach behavior is easier to exercise through
 * the simulator command layer.
 */
#include "flashwriter2.c"
#undef sim_map_resource
#undef malloc

static void cleanup_flashwriter_unit(UNIT *uptr)
{
    free(fw2_info[0]);
    fw2_info[0] = NULL;
    free(uptr->filename);
    uptr->filename = NULL;
    uptr->flags &= ~UNIT_ATT;
}

static void
test_attach_allocates_filename_storage_for_formatted_base(void **state)
{
    UNIT *uptr = &fw2_dev.units[0];
    t_stat result;

    (void)state;

    fw2_test_last_malloc_size = 0;
    result = fw2_attach(uptr, "0");

    assert_int_equal(result, SCPE_OK);
    assert_int_equal(fw2_test_last_malloc_size, strlen("0x0000") + 1);
    assert_string_equal(uptr->filename, "0x0000");

    cleanup_flashwriter_unit(uptr);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_attach_allocates_filename_storage_for_formatted_base),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
