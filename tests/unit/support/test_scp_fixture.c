#include "test_scp_fixture.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_simh_personality.h"

/* Initialize a multi-unit device for SCP lookup and naming tests. */
void simh_test_init_multiunit_device(DEVICE *device, UNIT *units,
                                     uint32_t numunits, const char *dev_name,
                                     const char *logical_name, uint32_t dev_flags)
{
    uint32_t i;

    memset(device, 0, sizeof(*device));
    memset(units, 0, numunits * sizeof(*units));

    device->name = dev_name;
    device->lname = (char *)logical_name;
    device->units = units;
    device->numunits = numunits;
    device->flags = dev_flags;

    for (i = 0; i < numunits; ++i)
        units[i].dptr = device;
}

/* Reset the shared simulator personality and install a device table. */
int simh_test_install_devices(const char *sim_name, DEVICE *const *devices)
{
    simh_test_reset_simulator_state();
    if (simh_test_set_sim_name(sim_name) != 0)
        return -1;
    if (simh_test_set_devices(devices) != 0)
        return -1;
    sim_switches = 0;
    sim_switch_number = 0;
    return 0;
}

/* Release any cached unit names allocated during SCP naming tests. */
void simh_test_free_unit_names(UNIT *units, uint32_t numunits)
{
    uint32_t i;

    for (i = 0; i < numunits; ++i) {
        free(units[i].uname);
        units[i].uname = NULL;
    }
}
