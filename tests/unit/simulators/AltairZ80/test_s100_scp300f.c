#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "altairz80_defs.h"

uint32_t PCX;

static int sio_status_calls;
static int32_t sio_status_port;
static int32_t sio_status_io;
static int32_t sio_status_data;

t_stat set_membase(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat show_membase(FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat set_iobase(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat show_iobase(FILE *st, UNIT *uptr, int32_t val, const void *desc);
uint32_t sim_map_resource(uint32_t baseaddr, uint32_t size,
                          uint32_t resource_type,
                          int32_t (*routine)(const int32_t, const int32_t,
                                             const int32_t),
                          const char *name, uint8_t unmap);
int32_t find_unit_index(UNIT *uptr);
int32_t sio0d(const int32_t port, const int32_t io, const int32_t data);
int32_t sio0s(const int32_t port, const int32_t io, const int32_t data);
void cpu_raise_interrupt(uint32_t irq);

t_stat set_membase(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_membase(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

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

int32_t find_unit_index(UNIT *uptr)
{
    (void)uptr;

    return 0;
}

int32_t sio0d(const int32_t port, const int32_t io, const int32_t data)
{
    (void)port;
    (void)io;
    (void)data;

    return 0;
}

int32_t sio0s(const int32_t port, const int32_t io, const int32_t data)
{
    sio_status_calls++;
    sio_status_port = port;
    sio_status_io = io;
    sio_status_data = data;

    return 0;
}

void cpu_raise_interrupt(uint32_t irq)
{
    (void)irq;
}

#include "s100_scp300f.c"

static void read_debug_output(FILE *stream, char *buf, size_t size)
{
    size_t n;

    assert_true(size > 0);
    sim_flush_buffered_files();
    rewind(stream);
    n = fread(buf, 1, size - 1, stream);
    assert_false(ferror(stream));
    buf[n] = '\0';
}

static void test_uart_status_write_does_not_touch_pio_data(void **state)
{
    FILE *debug_stream;
    char output[2048];
    FILE *saved_sim_deb;
    uint32_t saved_dctrl;

    (void)state;

    debug_stream = tmpfile();
    assert_non_null(debug_stream);

    sio_status_calls = 0;
    PCX = 0x12345;
    saved_sim_deb = sim_deb;
    saved_dctrl = scp300f_dev.dctrl;
    sim_deb = debug_stream;
    scp300f_dev.dctrl = UART_MSG | PIO_MSG;

    assert_int_equal(SCP300F_Write(SCP300F_UART_STATUS, 0x55), 0);
    read_debug_output(debug_stream, output, sizeof(output));

    sim_deb = saved_sim_deb;
    scp300f_dev.dctrl = saved_dctrl;
    assert_int_equal(fclose(debug_stream), 0);

    assert_int_equal(sio_status_calls, 1);
    assert_int_equal(sio_status_port, SCP300F_UART_STATUS);
    assert_int_equal(sio_status_io, 1);
    assert_int_equal(sio_status_data, 0x55);
    assert_non_null(strstr(output, "UART Stat=0x55"));
    assert_null(strstr(output, "PIO DATA WR"));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_uart_status_write_does_not_touch_pio_data),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
