/* vax_va_internal.h: Private helpers for the VCB02/QDSS simulator */

#ifndef VAX_VA_INTERNAL_H_
#define VAX_VA_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

#define VA_DGA_CSR_DMA_ERROR 0x0080u
#define VA_DGA_CSR_SLAVE_PARITY_ERR 0x0040u
#define VA_DGA_CSR_BUS_TIMEOUT_ERR 0x0020u
#define VA_DGA_CSR_ERROR_BITS 0x00E0u

typedef struct {
    uint32_t bytes;
    uint32_t words;
    bool bus_timeout;
} VA_DGA_MAP_STATUS;

/*
 * Convert a Qbus map routine's residual byte count into the number of bytes
 * the DMA gate array actually transferred. A non-zero residual means the
 * gate array encountered a Q22-bus timeout while it was bus master.
 */
static inline VA_DGA_MAP_STATUS va_dga_map_status(uint32_t requested_bytes,
                                                  int32_t residual_bytes)
{
    VA_DGA_MAP_STATUS status;
    uint32_t residual;

    residual = (residual_bytes < 0) ? 0 : (uint32_t)residual_bytes;
    if (residual > requested_bytes)
        residual = requested_bytes;

    status.bytes = requested_bytes - residual;
    status.words = status.bytes >> 1;
    status.bus_timeout = (residual != 0);
    return status;
}

/*
 * The VCB02 manual says a bus timeout while the DMA gate array is bus master
 * sets both the bus-timeout status bit and the aggregate DMA-error bit.
 */
static inline uint32_t va_dga_csr_set_bus_timeout(uint32_t csr)
{
    return csr | VA_DGA_CSR_DMA_ERROR | VA_DGA_CSR_BUS_TIMEOUT_ERR;
}

/*
 * Writing a one to the DMA-error bit clears all three DMA error status bits.
 * Writing zero to that bit leaves the existing error status unchanged.
 */
static inline uint32_t va_dga_csr_clear_dma_errors(uint32_t csr)
{
    return csr & ~VA_DGA_CSR_ERROR_BITS;
}

#endif /* VAX_VA_INTERNAL_H_ */
