#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "altairz80_defs.h"

uint32_t PCX;
int32_t vectorInterrupt;

t_stat set_iobase(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat show_iobase(FILE *st, UNIT *uptr, int32_t val, const void *desc);
uint32_t sim_map_resource(uint32_t baseaddr, uint32_t size,
                          uint32_t resource_type,
                          int32_t (*routine)(const int32_t, const int32_t,
                                             const int32_t),
                          const char *name, uint8_t unmap);
int32_t find_unit_index(UNIT *uptr);

t_stat set_iobase(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_iobase(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

uint32_t sim_map_resource(uint32_t baseaddr, uint32_t size,
                          uint32_t resource_type,
                          int32_t (*routine)(const int32_t, const int32_t,
                                             const int32_t),
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

#include "ibc_smd_hdc.c"

int32_t find_unit_index(UNIT *uptr)
{
    int32_t i;

    for (i = 0; i < IBC_SMD_MAX_DRIVES; i++) {
        if (uptr == &ibc_smd_unit[i])
            return i;
    }

    return -1;
}

static void reset_ibc_smd_register_state(void)
{
    ibc_smd_info = &ibc_smd_info_data;
    memset(ibc_smd_info, 0, sizeof(*ibc_smd_info));
}

static void test_unknown_read_port_does_not_consume_fifo_data(void **state)
{
    uint8_t value;

    (void)state;

    reset_ibc_smd_register_state();
    ibc_smd_info->secbuf_index = 3;
    ibc_smd_info->sectbuf[3] = 0xa5;

    value = IBC_SMD_Read(0x1);

    assert_int_equal(value, 0x7f);
    assert_int_equal(ibc_smd_info->secbuf_index, 3);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unknown_read_port_does_not_consume_fifo_data),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
