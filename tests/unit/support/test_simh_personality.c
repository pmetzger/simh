#include "test_simh_personality.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "scp.h"

#define SIMH_TEST_MAX_DEVICES 16

DEVICE *sim_devices[SIMH_TEST_MAX_DEVICES];
REG *sim_PC = NULL;
char sim_name[64] = "zimh-unit";
const char *sim_stop_messages[SCPE_BASE];
int32_t sim_emax = 0;

void simh_test_reset_simulator_state(void)
{
    memset(sim_devices, 0, sizeof(sim_devices));
    sim_PC = NULL;
    sim_prog_name = NULL;
    memset(sim_name, 0, sizeof(sim_name));
    memcpy(sim_name, "zimh-unit", sizeof("zimh-unit"));
    memset(&sim_stop_messages[0], 0, sizeof(sim_stop_messages));
    sim_emax = 0;
    sim_dfdev = NULL;
    sim_dfunit = NULL;
    sim_cmdvars_reset();
    free(sim_internal_devices);
    sim_internal_devices = NULL;
    sim_internal_device_count = 0;
}

int simh_test_set_sim_name(const char *name)
{
    size_t length;

    length = strlen(name);
    if (length >= sizeof(sim_name)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memset(sim_name, 0, sizeof(sim_name));
    memcpy(sim_name, name, length + 1);
    return 0;
}

int simh_test_set_devices(DEVICE *const *devices)
{
    size_t index;

    memset(sim_devices, 0, sizeof(sim_devices));

    for (index = 0; devices[index] != NULL; ++index) {
        if (index >= SIMH_TEST_MAX_DEVICES - 1) {
            errno = E2BIG;
            return -1;
        }
        sim_devices[index] = devices[index];
    }

    sim_devices[index] = NULL;
    return 0;
}

t_stat sim_instr(void)
{
    return SCPE_OK;
}

t_stat sim_load(FILE *ptr, const char *cptr, const char *fnam, int flag)
{
    (void)ptr;
    (void)cptr;
    (void)fnam;
    (void)flag;

    return SCPE_OK;
}

t_stat fprint_sym(FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32_t sw)
{
    (void)ofile;
    (void)addr;
    (void)val;
    (void)uptr;
    (void)sw;

    return SCPE_OK;
}

t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
                 int32_t sw)
{
    (void)cptr;
    (void)addr;
    (void)uptr;
    (void)val;
    (void)sw;

    return SCPE_OK;
}
