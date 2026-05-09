#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "vax_va_internal.h"

/*
 * A successful Qbus map transfer returns no residual byte count. The DGA
 * should account for the entire request as transferred words.
 */
static void test_map_status_accepts_complete_transfer(void **state)
{
    VA_DGA_MAP_STATUS status;

    (void)state;

    status = va_dga_map_status(128, 0);

    assert_int_equal(status.bytes, 128);
    assert_int_equal(status.words, 64);
    assert_false(status.bus_timeout);
}

/*
 * A non-zero residual count means the map layer stopped before transferring
 * the whole request. Only the completed byte range may be consumed by the
 * DGA path.
 */
static void test_map_status_reports_partial_bus_timeout(void **state)
{
    VA_DGA_MAP_STATUS status;

    (void)state;

    status = va_dga_map_status(128, 64);

    assert_int_equal(status.bytes, 64);
    assert_int_equal(status.words, 32);
    assert_true(status.bus_timeout);
}

/*
 * If the first mapped word fails, no bytes were transferred. The DGA must
 * leave the DMA address and byte counter unchanged when applying this result.
 */
static void test_map_status_reports_immediate_bus_timeout(void **state)
{
    VA_DGA_MAP_STATUS status;

    (void)state;

    status = va_dga_map_status(128, 128);

    assert_int_equal(status.bytes, 0);
    assert_int_equal(status.words, 0);
    assert_true(status.bus_timeout);
}

/*
 * The map helpers should never return more residual bytes than requested, but
 * clamping keeps the DMA accounting from underflowing if a bad caller or test
 * double violates that contract.
 */
static void test_map_status_clamps_oversized_residual(void **state)
{
    VA_DGA_MAP_STATUS status;

    (void)state;

    status = va_dga_map_status(128, 256);

    assert_int_equal(status.bytes, 0);
    assert_int_equal(status.words, 0);
    assert_true(status.bus_timeout);
}

/*
 * The VCB02 CSR records a Q22-bus timeout in bit 05 and summarizes any DMA
 * fault in bit 07. Other control bits must be preserved.
 */
static void test_csr_set_bus_timeout_preserves_control_bits(void **state)
{
    uint32_t csr;

    (void)state;

    csr = va_dga_csr_set_bus_timeout(0x4703u);

    assert_int_equal(csr & VA_DGA_CSR_DMA_ERROR, VA_DGA_CSR_DMA_ERROR);
    assert_int_equal(csr & VA_DGA_CSR_BUS_TIMEOUT_ERR,
                     VA_DGA_CSR_BUS_TIMEOUT_ERR);
    assert_int_equal(csr & 0x4703u, 0x4703u);
}

/*
 * The manual defines CSR bit 07 as write-one-to-clear for all DMA error
 * status bits: DMA error, slave parity error, and bus timeout.
 */
static void test_csr_clear_dma_errors_preserves_non_error_bits(void **state)
{
    uint32_t csr;

    (void)state;

    csr = va_dga_csr_clear_dma_errors(0x47E3u);

    assert_int_equal(csr & VA_DGA_CSR_ERROR_BITS, 0);
    assert_int_equal(csr & 0x4703u, 0x4703u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_map_status_accepts_complete_transfer),
        cmocka_unit_test(test_map_status_reports_partial_bus_timeout),
        cmocka_unit_test(test_map_status_reports_immediate_bus_timeout),
        cmocka_unit_test(test_map_status_clamps_oversized_residual),
        cmocka_unit_test(test_csr_set_bus_timeout_preserves_control_bits),
        cmocka_unit_test(test_csr_clear_dma_errors_preserves_non_error_bits),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
