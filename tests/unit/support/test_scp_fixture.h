#ifndef TEST_SCP_FIXTURE_H
#define TEST_SCP_FIXTURE_H

#include <stdint.h>

#include "sim_defs.h"

void simh_test_init_multiunit_device(DEVICE *device, UNIT *units,
                                     uint32_t numunits, const char *dev_name,
                                     const char *logical_name,
                                     uint32_t dev_flags);
int simh_test_install_devices(const char *sim_name, DEVICE *const *devices);
void simh_test_free_unit_names(UNIT *units, uint32_t numunits);

#endif
