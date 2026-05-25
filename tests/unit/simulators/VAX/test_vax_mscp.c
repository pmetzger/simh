#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_mscp.h"

#define TEST_MEM_WORDS 65536u
#define CMD_PACKET 0x00200020u
#define CMD_PACKET2 0x002000A0u
#define RSP_PACKET 0x00200060u
#define RSP_PACKET2 0x002000E0u
#define UQ_OWN 0x80000000u
#define UQ_FLAG 0x40000000u
#define UQ_PACKET_WORDS 32u
#define KDB_MAP 0x80000000u
#define STEP4_NODE_NAME 0x0200u
#define MD_COMPARE 0x4000u
#define MD_CSE 0x2000u
#define MD_FORCE_ERROR 0x1000u
#define VAX_PTE_KERNEL_RW 0xE0000000u
#define VAX_PTE_VALID 0x80000000u
#define VAX_PTE_PFN_MASK 0x001fffffu
#define VAX_PAGE_BYTES 512u
#define TEST_PHYS_ADDR_MASK 0x1fffffffu
#define TEST_BUS_ADDR_MAX 0x3fffffffu
#define TEST_MAPPED_MAPBASE_INVALID 0xc0000003u
#define TEST_MAPPED_DESCRIPTOR_MAX 0x7fffffffu
#define TEST_UNMAPPED_MAX_ADDR 0x3ffffffeu
#define TEST_FORCED_ERROR_BLOCKS 891072u
#define STEP4_KDB50 0x4123u
#define MEDIA_RA81_LOW 0x1051u
#define MEDIA_RA81_HIGH 0x2564u
#define MEDIA_RA81 ((MEDIA_RA81_HIGH << 16) | MEDIA_RA81_LOW)
#define RA81_MODEL 5u
#define RA81_SECTORS_PER_TRACK 51u
#define RA81_TRACKS_PER_GROUP 14u
#define RA81_GROUPS_PER_CYLINDER 1u
#define OP_GUS 3u
#define GUS_NEXT_UNIT 1u
#define OP_GETCMDST 2u
#define OP_SCC 4u
#define OP_DISPLAY 6u
#define OP_AVAILABLE 8u
#define OP_ONL 9u
#define OP_SETUNITC 10u
#define OP_DAP 11u
#define OP_DCD 13u
#define OP_ACC 16u
#define OP_ERS 18u
#define OP_REPLACE 20u
#define OP_FORMAT 24u
#define OP_WHM 25u
#define OP_NOP17 17u
#define OP_NOP19 19u
#define OP_CMP 32u
#define OP_RD 33u
#define OP_WR 34u
#define OP_TERMINATE 48u
#define OP_END 0x80u
#define MD_DISPLAY_MBI 0x0002u
#define MD_STWRP 0x0004u
#define MD_SHADOW_UNIT 0x0010u
#define UF_CMPRD 0x0001u
#define UF_CMPWR 0x0002u
#define UF_WRTPS 0x1000u
#define ST_SUCCESS 0u
#define ST_INVALID_ITEM 0x0c01u
#define ST_INVALID_MESSAGE_LENGTH 0x0001u
#define ST_INVALID_MODIFIER 0x0a01u
#define ST_INVALID_BYTE_COUNT 0x0c01u
#define ST_INVALID_LBN 0x1c01u
#define ST_INVALID_MSCP_VERSION 0x0c01u
#define ST_INVALID_OPCODE 0x0801u
#define ST_INVALID_BUFFER_RESERVED_FIELD 0x1001u
#define ST_INVALID_ONL_RESERVED_FIELD 0x0c01u
#define ST_INVALID_RESERVED_FIELD 0x0601u
#define ST_INVALID_SHADOW_RESERVED_FIELD 0x2001u
#define ST_AVAILABLE 4u
#define ST_COMPARE_ERROR 7u
#define ST_ALREADY_ONLINE 0x0100u
#define ST_DATA_ERROR_FORCE_ERROR 8u
#define ST_HOST_BUFFER_ACCESS 9u
#define ST_HOST_BUFFER_ODD_ADDRESS 0x0029u
#define ST_HOST_BUFFER_ODD_BYTE_COUNT 0x0049u
#define ST_HOST_BUFFER_INVALID_PTE 0x00a9u
#define ST_WRITE_PROTECTED_SOFTWARE 0x1006u
#define ST_OFFLINE 3u
#define UQ_TYPE_DATAGRAM 1u
#define UQ_TYPE_CREDIT_NOTIFICATION 2u
#define UQ_CID_TAPE 1u
#define LF_SEQUENCE_RESET 0x0001u
#define LF_FORMAT_CONTROLLER_ERROR 0u
#define LF_EVENT_CONTROLLER_ERROR 10u
#define SCC_FLAGS 9u
#define SCC_TIMEOUT 10u
#define SCC_VERSION 11u
#define SCC_CIDA 12u
#define SCC_CIDB 13u
#define SCC_CIDC 14u
#define SCC_CIDD 15u
#define SCC_MAX_BYTE_COUNT_LOW 16u
#define SCC_MAX_BYTE_COUNT_HIGH 17u
#define SCC_CF_RPL 0x8000u
#define SCC_CF_HOST_SETTABLE 0x00f0u
#define SCC_CF_UNSUPPORTED_OPTIONAL 0x070fu
#define SCC_CONTROLLER_CLASS 1u
#define SCC_KDB50_MODEL 18u
#define SCC_HW_VERSION 1u
#define SCC_SW_VERSION 3u
#define SCC_DEFAULT_TIMEOUT 120u
#define MSCP_UNIT_FLAGS 9u
#define MSCP_UNIT_UIDB 13u
#define MSCP_UNIT_UIDC 14u
#define MSCP_UNIT_UIDD 15u
#define MSCP_UNIT_SHADOW_UNIT 18u
#define MSCP_UNIT_SHADOW_STATUS 19u
#define GUS_MULTILUN_UNIT 8u
#define GUS_TRACKS 20u
#define GUS_GROUPS 21u
#define GUS_CYLINDERS 22u
#define GUS_UNIT_VERSION 23u
#define OP_ABORT 1u
#define ONL_MULTILUN_UNIT 8u
#define GCS_OUTSTANDING_REF_LOW 8u
#define GCS_CMD_STATUS_LOW 10u
#define FATAL_PACKET_READ 0x8001u
#define FATAL_PACKET_WRITE 0x8002u
#define FATAL_QUEUE_READ 0x8006u
#define FATAL_INVALID_CONNECTION_ID 0x800eu
#define FATAL_INTERRUPT_WRITE 0x800fu
#define FATAL_PROTOCOL_INCOMPATIBILITY 0x8014u

static const vax_mscp_profile test_profile = {
    .controller_model = SCC_KDB50_MODEL,
    .controller_hw_version = SCC_HW_VERSION,
    .controller_sw_version = SCC_SW_VERSION,
    .controller_timeout = SCC_DEFAULT_TIMEOUT,
    .max_units = VAX_MSCP_MAX_UNITS,
    .unit_model = RA81_MODEL,
    .sectors_per_track = RA81_SECTORS_PER_TRACK,
    .tracks_per_group = RA81_TRACKS_PER_GROUP,
    .groups_per_cylinder = RA81_GROUPS_PER_CYLINDER,
    .media_id = MEDIA_RA81,
    .buffer_map_bit = KDB_MAP,
    .phys_addr_mask = TEST_PHYS_ADDR_MASK,
    .bus_addr_max = TEST_BUS_ADDR_MAX,
    .mapped_mapbase_invalid = TEST_MAPPED_MAPBASE_INVALID,
    .mapped_descriptor_max = TEST_MAPPED_DESCRIPTOR_MAX,
    .unmapped_max_addr = TEST_UNMAPPED_MAX_ADDR,
    .pte_valid = VAX_PTE_VALID,
    .pte_pfn_mask = VAX_PTE_PFN_MASK,
    .page_bytes = VAX_PAGE_BYTES,
    .forced_error_blocks = TEST_FORCED_ERROR_BLOCKS,
};

typedef struct {
    uint32_t base;
    uint16_t words[TEST_MEM_WORDS];
    uint8_t sector[64 * 1024];
    uint32_t map_pte[128];
    uint32_t writes;
    uint32_t map_reads;
    uint32_t fail_word_read_addr;
    uint32_t fail_word_write_addr;
    uint16_t expected_unit;
    uint32_t expected_lbn;
    uint32_t expected_sectors;
    uint32_t sectors_read;
    uint32_t sectors_written;
    uint32_t ring_interrupts;
    uint8_t corrupt_disk_writes;
    uint8_t corrupt_host_writes;
    uint8_t fail_word_reads;
    uint8_t fail_word_writes;
    uint8_t repeat_reads;
} FakeMemory;

static int fake_read_words(void *ctx, uint32_t addr, uint16_t *buf,
                           uint32_t bytes)
{
    FakeMemory *mem = ctx;
    uint32_t index;

    assert_true((bytes & 1u) == 0);
    assert_true(addr >= mem->base);
    if ((mem->fail_word_reads == 2) && (addr == mem->fail_word_read_addr))
        return 1;
    index = (addr - mem->base) >> 1;
    assert_true(index + (bytes >> 1) <= TEST_MEM_WORDS);
    memcpy(buf, &mem->words[index], bytes);
    return 0;
}

static int fake_read_words_fails_high_addr(void *ctx, uint32_t addr,
                                           uint16_t *buf, uint32_t bytes)
{
    FakeMemory *mem = ctx;

    if ((uint64_t)addr + bytes - 1u > TEST_BUS_ADDR_MAX) {
        mem->map_reads++;
        return 1;
    }

    return fake_read_words(ctx, addr, buf, bytes);
}

static int fake_write_words(void *ctx, uint32_t addr, const uint16_t *buf,
                            uint32_t bytes)
{
    FakeMemory *mem = ctx;
    uint32_t index;

    assert_true((bytes & 1u) == 0);
    assert_true(addr >= mem->base);
    if (mem->fail_word_writes == 1)
        return 1;
    if ((mem->fail_word_writes == 2) && (addr == mem->fail_word_write_addr))
        return 1;
    index = (addr - mem->base) >> 1;
    assert_true(index + (bytes >> 1) <= TEST_MEM_WORDS);
    memcpy(&mem->words[index], buf, bytes);
    mem->writes++;
    return 0;
}

static int fake_read_bytes(void *ctx, uint32_t addr, uint8_t *buf,
                           uint32_t bytes)
{
    FakeMemory *mem = ctx;

    assert_true(addr >= mem->base);
    assert_true(addr + bytes <= mem->base + (TEST_MEM_WORDS * 2u));
    memcpy(buf, ((uint8_t *)mem->words) + (addr - mem->base), bytes);
    return 0;
}

static int fake_write_bytes(void *ctx, uint32_t addr, const uint8_t *buf,
                            uint32_t bytes)
{
    FakeMemory *mem = ctx;

    assert_true(addr >= mem->base);
    assert_true(addr + bytes <= mem->base + (TEST_MEM_WORDS * 2u));
    memcpy(((uint8_t *)mem->words) + (addr - mem->base), buf, bytes);
    if (mem->corrupt_host_writes && bytes > 0)
        ((uint8_t *)mem->words)[addr - mem->base] ^= 0xffu;
    return 0;
}

static int fake_read_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                             uint8_t *buf, uint32_t sectors)
{
    FakeMemory *mem = ctx;

    assert_int_equal(unit, mem->expected_unit);
    if (mem->repeat_reads)
        assert_int_equal(lbn, mem->expected_lbn +
                                  (mem->sectors_read % mem->expected_sectors));
    else
        assert_int_equal(lbn, mem->expected_lbn + mem->sectors_read);
    assert_int_equal(sectors, 1);
    assert_true((mem->sectors_read % mem->expected_sectors) + sectors <=
                mem->expected_sectors);
    memcpy(buf,
           &mem->sector[(mem->sectors_read % mem->expected_sectors) *
                        VAX_PAGE_BYTES],
           sectors * VAX_PAGE_BYTES);
    mem->sectors_read += sectors;
    return 0;
}

static int fake_read_map_pte(void *ctx, uint32_t mapbase, uint32_t page_index,
                             uint32_t *pte)
{
    FakeMemory *mem = ctx;

    assert_int_equal(mapbase, 0x002001f8u);
    assert_true(page_index < 128u);
    mem->map_reads++;
    *pte = mem->map_pte[page_index];
    return 0;
}

static int fake_read_any_map_pte(void *ctx, uint32_t mapbase,
                                 uint32_t page_index, uint32_t *pte)
{
    FakeMemory *mem = ctx;

    (void)mapbase;

    assert_true(page_index < 128u);
    mem->map_reads++;
    *pte = mem->map_pte[page_index];
    return 0;
}

static int fake_write_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                              const uint8_t *buf, uint32_t sectors)
{
    FakeMemory *mem = ctx;

    assert_int_equal(unit, mem->expected_unit);
    assert_int_equal(lbn, mem->expected_lbn + mem->sectors_written);
    assert_int_equal(sectors, 1);
    assert_true(mem->sectors_written + sectors <= mem->expected_sectors);
    memcpy(&mem->sector[mem->sectors_written * VAX_PAGE_BYTES], buf,
           sectors * VAX_PAGE_BYTES);
    if (mem->corrupt_disk_writes && sectors > 0)
        mem->sector[mem->sectors_written * VAX_PAGE_BYTES] ^= 0xffu;
    mem->sectors_written += sectors;
    return 0;
}

static void fake_ring_interrupt(void *ctx)
{
    FakeMemory *mem = ctx;

    mem->ring_interrupts++;
}

static void write_desc(FakeMemory *mem, uint32_t addr, uint32_t desc);
static uint32_t read_desc(FakeMemory *mem, uint32_t addr);
static uint16_t *packet_at(FakeMemory *mem, uint32_t packet_addr);
static void bring_controller_up(vax_mscp *ctlr);

static void test_init_uses_callback_memory_for_comm_region(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0xa5, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.writes = 0;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x0940);

    vax_mscp_write_sa(&ctlr, 0x8080);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x1080);

    vax_mscp_write_sa(&ctlr, 0x0000);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x2080);

    vax_mscp_write_sa(&ctlr, 0x0020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
    assert_int_equal(vax_mscp_comm_region(&ctlr), 0x00200000);
    assert_int_equal(mem.writes, 1);

    assert_int_equal(mem.words[0], 0);
    assert_int_equal(mem.words[1], 0);
    assert_int_equal(mem.words[2], 0);
    assert_int_equal(mem.words[3], 0);
}

static void test_init_clears_entire_comm_region_for_large_rings(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0xa5, sizeof(mem));
    mem.base = 0x00200000u - 4u;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0xbf80);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);

    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
    for (uint32_t i = 0; i < 1024u / sizeof(mem.words[0]); i++)
        assert_int_equal(mem.words[i], 0);
}

static void test_diagnostic_wrap_echoes_sa_until_reset(void **state)
{
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
    };

    (void)state;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);

    vax_mscp_write_sa(&ctlr, 0xc080);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0xc080);

    vax_mscp_write_sa(&ctlr, 0x1234);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x1234);

    vax_mscp_reset(&ctlr);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x0940);
}

static void
test_purge_poll_reaches_step4_after_zero_sa_and_ip_read(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0xa5, sizeof(mem));
    mem.base = 0x00200000u - 8u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);

    vax_mscp_write_sa(&ctlr, 0x80a4);
    vax_mscp_write_sa(&ctlr, 0x0001);
    vax_mscp_write_sa(&ctlr, 0x8020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0);

    vax_mscp_write_sa(&ctlr, 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0);

    mem.ring_interrupts = 0;
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
    assert_int_equal(mem.ring_interrupts, 1);
    for (uint32_t i = 0; i < 16u / sizeof(mem.words[0]); i++)
        assert_int_equal(mem.words[i], 0);
}

static void test_purge_poll_nonzero_sa_reports_purge_poll_failure(void **state)
{
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
    };

    (void)state;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);

    vax_mscp_write_sa(&ctlr, 0x80a4);
    vax_mscp_write_sa(&ctlr, 0x0001);
    vax_mscp_write_sa(&ctlr, 0x8020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0);

    vax_mscp_write_sa(&ctlr, 1);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x8015);
}

static void test_init_reports_fatal_when_comm_region_write_fails(void **state)
{
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
    };

    (void)state;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);

    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x8007);
}

static void assert_last_fail_packet(const uint16_t *rsp, uint16_t fatal_code)
{
    assert_int_equal(rsp[0], 24);
    assert_int_equal(rsp[1], UQ_TYPE_DATAGRAM << 4);
    assert_int_equal(rsp[2], 0);
    assert_int_equal(rsp[3], 0);
    assert_int_equal(rsp[4], 0);
    assert_int_equal(rsp[5], 0);
    assert_int_equal(rsp[6],
                     (LF_SEQUENCE_RESET << 8) | LF_FORMAT_CONTROLLER_ERROR);
    assert_int_equal(rsp[7], LF_EVENT_CONTROLLER_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(rsp[10], 0);
    assert_int_equal(rsp[11], (SCC_CONTROLLER_CLASS << 8) | SCC_KDB50_MODEL);
    assert_int_equal(rsp[12], (SCC_HW_VERSION << 8) | SCC_SW_VERSION);
    assert_int_equal(rsp[13], fatal_code);
}

static void test_last_fail_survives_until_lf_initialization(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.fail_word_writes = 1;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x8007);

    mem.fail_word_writes = 0;
    bring_controller_up(&ctlr);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200000u), 0);

    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    vax_mscp_write_sa(&ctlr, 0x0003);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_FLAG | RSP_PACKET);
    assert_last_fail_packet(packet_at(&mem, RSP_PACKET), 7);

    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET2);
    vax_mscp_write_sa(&ctlr, 0x0003);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_OWN | RSP_PACKET2);
}

static void test_last_fail_waits_for_response_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.fail_word_writes = 1;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x8007);

    mem.fail_word_writes = 0;
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    vax_mscp_write_sa(&ctlr, 0x0003);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200000u), 0);

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_FLAG | RSP_PACKET);
    assert_last_fail_packet(packet_at(&mem, RSP_PACKET), 7);
}

static void test_init_step3_echoes_ie_and_vector_from_step1(void **state)
{
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
    };

    (void)state;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0xadd5);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x10ad);

    vax_mscp_write_sa(&ctlr, 0xe008);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x20d5);
}

static void write_desc(FakeMemory *mem, uint32_t addr, uint32_t desc)
{
    uint32_t index = (addr - mem->base) >> 1;

    mem->words[index] = desc & 0xffffu;
    mem->words[index + 1] = desc >> 16;
}

static uint32_t read_desc(FakeMemory *mem, uint32_t addr)
{
    uint32_t index = (addr - mem->base) >> 1;

    return mem->words[index] | ((uint32_t)mem->words[index + 1] << 16);
}

static uint16_t *packet_at(FakeMemory *mem, uint32_t packet_addr)
{
    return &mem->words[(packet_addr - 4u - mem->base) >> 1];
}

static uint16_t *command_packet_at(FakeMemory *mem, uint32_t packet_addr)
{
    uint16_t *cmd = packet_at(mem, packet_addr);

    memset(cmd, 0, UQ_PACKET_WORDS * sizeof(cmd[0]));
    cmd[0] = UQ_PACKET_WORDS * sizeof(cmd[0]);
    return cmd;
}

static void bring_controller_up(vax_mscp *ctlr)
{
    vax_mscp_reset(ctlr);
    vax_mscp_write_sa(ctlr, 0x8080);
    vax_mscp_write_sa(ctlr, 0x0000);
    vax_mscp_write_sa(ctlr, 0x0020);
    vax_mscp_write_sa(ctlr, 0x0001);
}

static void bring_controller_up_at(vax_mscp *ctlr, uint32_t comm)
{
    vax_mscp_reset(ctlr);
    vax_mscp_write_sa(ctlr, 0x8080);
    vax_mscp_write_sa(ctlr, comm & 0xfffeu);
    vax_mscp_write_sa(ctlr, (comm >> 16) & 0x7fffu);
    vax_mscp_write_sa(ctlr, 0x0001);
}

static void bring_controller_up_with_two_entry_rings(vax_mscp *ctlr)
{
    vax_mscp_reset(ctlr);
    vax_mscp_write_sa(ctlr, 0x8980);
    vax_mscp_write_sa(ctlr, 0x0000);
    vax_mscp_write_sa(ctlr, 0x0020);
    vax_mscp_write_sa(ctlr, 0x0001);
}

static void bring_controller_up_with_vector(vax_mscp *ctlr)
{
    vax_mscp_reset(ctlr);
    vax_mscp_write_sa(ctlr, 0x80a4);
    vax_mscp_write_sa(ctlr, 0x0000);
    vax_mscp_write_sa(ctlr, 0x0020);
    vax_mscp_write_sa(ctlr, 0x0001);
}

static uint16_t *run_simple_command(FakeMemory *mem, vax_mscp *ctlr,
                                    uint16_t opcode)
{
    uint16_t *cmd;

    write_desc(mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(mem, CMD_PACKET);
    cmd[6] = opcode;

    assert_int_equal(vax_mscp_read_ip(ctlr), 0);
    return packet_at(mem, RSP_PACKET);
}

static void test_init_interrupts_at_steps_1_to_3(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .write_words = fake_write_words,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);

    vax_mscp_write_sa(&ctlr, 0x80a4);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x1080);
    assert_int_equal(vax_mscp_interrupt_vector(&ctlr), 0x0090);
    assert_int_equal(mem.ring_interrupts, 1);

    vax_mscp_write_sa(&ctlr, 0x0000);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x20a4);
    assert_int_equal(mem.ring_interrupts, 2);

    vax_mscp_write_sa(&ctlr, 0x0020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
    assert_int_equal(mem.ring_interrupts, 3);
}

static void test_step4_go_zero_ignores_all_other_host_options(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;

    vax_mscp_write_sa(&ctlr, 0x03fe);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200004u), UQ_OWN | CMD_PACKET);
    assert_int_equal(mem.ring_interrupts, 0);
}

static void test_step4_unsupported_sfm_enters_normal_operation(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    assert_false(vax_mscp_read_sa(&ctlr) & 0x0020u);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;

    vax_mscp_write_sa(&ctlr, 0x03ff);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
    assert_int_equal(rsp[7], 0);
}

static void test_step4_ignores_node_name_when_not_advertised(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    assert_false(vax_mscp_read_sa(&ctlr) & 0x0010u);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    vax_mscp_write_sa(&ctlr, STEP4_NODE_NAME | 0x0001u);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_OWN | RSP_PACKET);
}

static void test_ip_poll_rejects_nonsequential_command_packet(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[1] = UQ_TYPE_CREDIT_NOTIFICATION << 4;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_PROTOCOL_INCOMPATIBILITY);
}

static void test_ip_poll_rejects_non_disk_mscp_connection_id(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[1] = UQ_CID_TAPE << 8;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_INVALID_CONNECTION_ID);
}

static void test_ip_poll_reports_queue_read_failure(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    mem.fail_word_reads = 2;
    mem.fail_word_read_addr = 0x00200004u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_QUEUE_READ);
}

static void test_ip_poll_reports_packet_read_failure(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    mem.fail_word_reads = 2;
    mem.fail_word_read_addr = CMD_PACKET - 4u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_PACKET_READ);
}

static void test_ip_poll_reports_packet_write_failure(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    mem.fail_word_writes = 2;
    mem.fail_word_write_addr = RSP_PACKET - 4u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_PACKET_WRITE);
}

static void test_ip_poll_reports_command_interrupt_write_failure(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | UQ_FLAG | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    mem.fail_word_writes = 2;
    mem.fail_word_write_addr = 0x00200000u - 4u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_INTERRUPT_WRITE);
}

static void test_ip_poll_reports_response_interrupt_write_failure(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    mem.fail_word_writes = 2;
    mem.fail_word_write_addr = 0x00200000u - 2u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    assert_int_equal(vax_mscp_read_sa(&ctlr), FATAL_INTERRUPT_WRITE);
}

static void test_zero_vector_suppresses_port_interrupts(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .write_words = fake_write_words,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);

    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);

    assert_int_equal(vax_mscp_interrupt_vector(&ctlr), 0);
    assert_int_equal(mem.ring_interrupts, 0);
}

static void test_ip_poll_completes_set_controller_characteristics(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_with_vector(&ctlr);
    mem.ring_interrupts = 0;

    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[2] = 0x3412;
    cmd[3] = 0x7856;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_FLAG | RSP_PACKET);
    assert_int_equal(read_desc(&mem, 0x00200004u), UQ_FLAG | CMD_PACKET);
    assert_int_equal(mem.words[1], 1);
    assert_int_equal(mem.ring_interrupts, 1);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[1] & 0x00ffu, 2);
    assert_int_equal(rsp[2], 0x3412);
    assert_int_equal(rsp[3], 0x7856);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[SCC_FLAGS], SCC_CF_RPL);
    assert_int_equal(rsp[SCC_TIMEOUT], SCC_DEFAULT_TIMEOUT);
    assert_int_equal(rsp[SCC_VERSION], (SCC_HW_VERSION << 8) | SCC_SW_VERSION);
    assert_int_equal(rsp[SCC_CIDA], 0);
    assert_int_equal(rsp[SCC_CIDB], 0);
    assert_int_equal(rsp[SCC_CIDC], 0);
    assert_int_equal(rsp[SCC_CIDD],
                     (SCC_CONTROLLER_CLASS << 8) | SCC_KDB50_MODEL);
    assert_int_equal(rsp[SCC_MAX_BYTE_COUNT_LOW], 0);
    assert_int_equal(rsp[SCC_MAX_BYTE_COUNT_HIGH], 0);
}

static void
test_ip_poll_set_controller_characteristics_rejects_mscp_version(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    cmd[8] = 1;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MSCP_VERSION);
}

static void
test_ip_poll_set_controller_characteristics_rejects_short_message(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[0] = 30;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MESSAGE_LENGTH);
}

static void
test_ip_poll_set_controller_characteristics_ignores_optional_flags(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    cmd[SCC_FLAGS] = SCC_CF_HOST_SETTABLE | SCC_CF_UNSUPPORTED_OPTIONAL;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(rsp[SCC_FLAGS], SCC_CF_RPL | SCC_CF_HOST_SETTABLE);
}

static void
test_ip_poll_suppresses_response_interrupt_without_flag(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_with_vector(&ctlr);
    mem.ring_interrupts = 0;

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_FLAG | RSP_PACKET);
    assert_int_equal(mem.words[1], 0);
    assert_int_equal(mem.ring_interrupts, 0);
}

static void
test_ip_poll_posts_command_transition_when_ring_was_full(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_with_vector(&ctlr);
    mem.ring_interrupts = 0;

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | UQ_FLAG | CMD_PACKET);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, 0x00200004u), UQ_FLAG | CMD_PACKET);
    assert_int_equal(mem.words[0], 1);
    assert_int_equal(mem.words[1], 0);
    assert_int_equal(mem.ring_interrupts, 1);
}

static void test_ip_poll_posts_only_first_response_transition(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x89a4);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0x0020);
    vax_mscp_write_sa(&ctlr, 0x0001);
    mem.ring_interrupts = 0;

    write_desc(&mem, 0x00200000u, UQ_OWN | UQ_FLAG | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | UQ_FLAG | RSP_PACKET2);
    write_desc(&mem, 0x00200008u, UQ_OWN | CMD_PACKET);
    write_desc(&mem, 0x0020000Cu, UQ_OWN | CMD_PACKET2);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SCC;
    cmd = command_packet_at(&mem, CMD_PACKET2);
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(mem.words[1], 1);
    assert_int_equal(mem.ring_interrupts, 1);
}

static void test_ip_poll_preserves_high_descriptor_addresses(void **state)
{
    enum {
        HIGH_COMM = 0x007D0000u,
        HIGH_CMD_PACKET = 0x007D0020u,
        HIGH_RSP_PACKET = 0x007D0060u,
    };
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = HIGH_COMM - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_at(&ctlr, HIGH_COMM);

    write_desc(&mem, HIGH_COMM, UQ_OWN | HIGH_RSP_PACKET);
    write_desc(&mem, HIGH_COMM + 4u, UQ_OWN | HIGH_CMD_PACKET);

    cmd = command_packet_at(&mem, HIGH_CMD_PACKET);
    cmd[2] = 0x3412;
    cmd[3] = 0x7856;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, HIGH_COMM), UQ_FLAG | HIGH_RSP_PACKET);
    assert_int_equal(read_desc(&mem, HIGH_COMM + 4u),
                     UQ_FLAG | HIGH_CMD_PACKET);

    rsp = packet_at(&mem, HIGH_RSP_PACKET);
    assert_int_equal(rsp[2], 0x3412);
    assert_int_equal(rsp[3], 0x7856);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
}

static void test_init_ignores_vax_pte_bits_in_ring_base(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0xa5, sizeof(mem));
    mem.base = 0x00080000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_reset(&ctlr);
    vax_mscp_write_sa(&ctlr, 0x8080);
    vax_mscp_write_sa(&ctlr, 0x0000);
    vax_mscp_write_sa(&ctlr, 0xe008);

    assert_int_equal(vax_mscp_comm_region(&ctlr), 0x00080000u);
    assert_int_equal(vax_mscp_read_sa(&ctlr), STEP4_KDB50);
}

static void test_ip_poll_ignores_vax_pte_bits_in_descriptors(void **state)
{
    enum {
        COMM = 0x00080000u,
        PTE_CMD_PACKET = 0x00080020u,
        PTE_RSP_PACKET = 0x00080060u,
    };
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = COMM - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_at(&ctlr, COMM);

    write_desc(&mem, COMM, UQ_OWN | VAX_PTE_KERNEL_RW | PTE_RSP_PACKET);
    write_desc(&mem, COMM + 4u, UQ_OWN | VAX_PTE_KERNEL_RW | PTE_CMD_PACKET);

    cmd = command_packet_at(&mem, PTE_CMD_PACKET);
    cmd[2] = 0x3412;
    cmd[3] = 0x7856;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, COMM), UQ_FLAG | PTE_RSP_PACKET);
    assert_int_equal(read_desc(&mem, COMM + 4u), UQ_FLAG | PTE_CMD_PACKET);

    rsp = packet_at(&mem, PTE_RSP_PACKET);
    assert_int_equal(rsp[2], 0x3412);
    assert_int_equal(rsp[3], 0x7856);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
}

static void test_ip_poll_drains_available_command_ring_entries(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up_with_two_entry_rings(&ctlr);

    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | RSP_PACKET2);
    write_desc(&mem, 0x00200008u, UQ_OWN | CMD_PACKET);
    write_desc(&mem, 0x0020000Cu, UQ_OWN | CMD_PACKET2);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[2] = 1;
    cmd[6] = OP_SCC;
    cmd = command_packet_at(&mem, CMD_PACKET2);
    cmd[2] = 2;
    cmd[6] = OP_SCC;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    assert_int_equal(read_desc(&mem, 0x00200008u), UQ_FLAG | CMD_PACKET);
    assert_int_equal(read_desc(&mem, 0x0020000Cu), UQ_FLAG | CMD_PACKET2);
    assert_int_equal(read_desc(&mem, 0x00200000u), UQ_FLAG | RSP_PACKET);
    assert_int_equal(read_desc(&mem, 0x00200004u), UQ_FLAG | RSP_PACKET2);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[2], 1);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
    rsp = packet_at(&mem, RSP_PACKET2);
    assert_int_equal(rsp[2], 2);
    assert_int_equal(rsp[6], OP_SCC | OP_END);
}

static uint16_t *setup_single_command(vax_mscp *ctlr, FakeMemory *mem,
                                      uint16_t opcode)
{
    uint16_t *cmd;

    bring_controller_up(ctlr);
    write_desc(mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(mem, 0x00200004u, UQ_OWN | CMD_PACKET);
    cmd = command_packet_at(mem, CMD_PACKET);
    cmd[2] = 0x3412;
    cmd[3] = 0x7856;
    cmd[4] = 0;
    cmd[6] = opcode;
    return cmd;
}

static void test_profile_validation_rejects_bad_shapes(void **state)
{
    vax_mscp_profile profile = test_profile;

    (void)state;

    assert_true(vax_mscp_profile_valid(&profile));
    assert_false(vax_mscp_profile_valid(NULL));

    profile = test_profile;
    profile.max_units = 0;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.max_units = VAX_MSCP_MAX_UNITS + 1u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.page_bytes = 0;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.page_bytes = 768u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.buffer_map_bit = 0xc0000000u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.mapped_descriptor_max = KDB_MAP;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.unmapped_max_addr = KDB_MAP;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.unmapped_max_addr = TEST_UNMAPPED_MAX_ADDR | 1u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.phys_addr_mask = TEST_BUS_ADDR_MAX + 1u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.controller_model = 0x100u;
    assert_false(vax_mscp_profile_valid(&profile));

    profile = test_profile;
    profile.forced_error_blocks = VAX_MSCP_FORCED_ERROR_MAX_BLOCKS + 1u;
    assert_false(vax_mscp_profile_valid(&profile));
}

static void test_init_and_reset_reject_invalid_profiles(void **state)
{
    vax_mscp ctlr;
    vax_mscp_profile profile = test_profile;
    vax_mscp_bus bus = {
        .profile = &profile,
    };

    (void)state;

    profile.max_units = 0;
    memset(&ctlr, 0, sizeof(ctlr));
    assert_false(vax_mscp_init(NULL, &bus));
    assert_false(vax_mscp_init(&ctlr, NULL));
    assert_false(vax_mscp_init(&ctlr, &bus));

    profile = test_profile;
    assert_true(vax_mscp_init(&ctlr, &bus));
    assert_true(vax_mscp_reset(&ctlr));

    profile.max_units = 0;
    assert_false(vax_mscp_reset(&ctlr));
    assert_false(vax_mscp_reset_with_bus(&ctlr, &bus));
}

static void test_reset_with_bus_preserves_units_and_last_fail(void **state)
{
    vax_mscp ctlr;
    uint32_t replacement_ctx = 1;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
    };
    const vax_mscp_bus replacement = {
        .profile = &test_profile,
        .ctx = &replacement_ctx,
    };

    (void)state;

    assert_true(vax_mscp_init(&ctlr, &bus));
    ctlr.unit[2].present = 1;
    ctlr.unit[2].blocks = 12345u;
    ctlr.last_fail_code = 7u;
    ctlr.last_fail_valid = 1u;

    assert_true(vax_mscp_reset_with_bus(&ctlr, &replacement));

    assert_ptr_equal(ctlr.bus.ctx, &replacement_ctx);
    assert_true(ctlr.unit[2].present);
    assert_int_equal(ctlr.unit[2].blocks, 12345u);
    assert_int_equal(ctlr.last_fail_code, 7u);
    assert_true(ctlr.last_fail_valid);
    assert_int_equal(vax_mscp_read_sa(&ctlr), 0x0940u);
}

static void test_ip_poll_reports_present_unit_status(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    (void)setup_single_command(&ctlr, &mem, OP_GUS);

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 48);
    assert_int_equal(rsp[4], 0);
    assert_int_equal(rsp[6], OP_GUS | OP_END);
    assert_int_equal(rsp[7], ST_AVAILABLE);
    assert_int_equal(rsp[GUS_MULTILUN_UNIT], 0);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000);
    assert_int_equal(rsp[16], MEDIA_RA81_LOW);
    assert_int_equal(rsp[17], MEDIA_RA81_HIGH);
    assert_int_equal(rsp[MSCP_UNIT_UIDB], 0);
    assert_int_equal(rsp[MSCP_UNIT_UIDC], 0);
    assert_int_equal(rsp[MSCP_UNIT_UIDD], (2u << 8) | RA81_MODEL);
    assert_int_equal(rsp[MSCP_UNIT_SHADOW_UNIT], 0);
    assert_int_equal(rsp[MSCP_UNIT_SHADOW_STATUS], 0);
    assert_int_equal(rsp[GUS_TRACKS], RA81_SECTORS_PER_TRACK);
    assert_int_equal(rsp[GUS_GROUPS], RA81_TRACKS_PER_GROUP);
    assert_int_equal(rsp[GUS_CYLINDERS], RA81_GROUPS_PER_CYLINDER);
    assert_int_equal(rsp[GUS_UNIT_VERSION], 0);
}

static void test_profile_supplies_controller_and_unit_identity(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    static const vax_mscp_profile profile = {
        .controller_model = 0x22u,
        .controller_hw_version = 0x33u,
        .controller_sw_version = 0x44u,
        .controller_timeout = 55u,
        .max_units = VAX_MSCP_MAX_UNITS,
        .unit_model = 0x66u,
        .sectors_per_track = 17u,
        .tracks_per_group = 3u,
        .groups_per_cylinder = 2u,
        .unit_version = 0x77u,
        .media_id = 0x11223344u,
        .buffer_map_bit = KDB_MAP,
        .phys_addr_mask = TEST_PHYS_ADDR_MASK,
        .bus_addr_max = TEST_BUS_ADDR_MAX,
        .mapped_mapbase_invalid = TEST_MAPPED_MAPBASE_INVALID,
        .mapped_descriptor_max = TEST_MAPPED_DESCRIPTOR_MAX,
        .unmapped_max_addr = TEST_UNMAPPED_MAX_ADDR,
        .pte_valid = VAX_PTE_VALID,
        .pte_pfn_mask = VAX_PTE_PFN_MASK,
        .page_bytes = VAX_PAGE_BYTES,
        .forced_error_blocks = TEST_FORCED_ERROR_BLOCKS,
    };
    const vax_mscp_bus bus = {
        .profile = &profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);

    (void)setup_single_command(&ctlr, &mem, OP_SCC);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[SCC_TIMEOUT], 55u);
    assert_int_equal(rsp[SCC_VERSION], (0x33u << 8) | 0x44u);
    assert_int_equal(rsp[SCC_CIDD], (SCC_CONTROLLER_CLASS << 8) | 0x22u);

    (void)setup_single_command(&ctlr, &mem, OP_GUS);
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[16], 0x3344u);
    assert_int_equal(rsp[17], 0x1122u);
    assert_int_equal(rsp[MSCP_UNIT_UIDD], (2u << 8) | 0x66u);
    assert_int_equal(rsp[GUS_TRACKS], 17u);
    assert_int_equal(rsp[GUS_GROUPS], 3u);
    assert_int_equal(rsp[GUS_CYLINDERS], 2u);
    assert_int_equal(rsp[GUS_UNIT_VERSION], 0x77u);
}

static void test_profile_limits_visible_units(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    vax_mscp_profile profile = test_profile;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    profile.max_units = 1u;
    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    vax_mscp_set_unit(&ctlr, 1, 12345);

    cmd = setup_single_command(&ctlr, &mem, OP_GUS);
    cmd[4] = 1u;
    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_GUS | OP_END);
    assert_int_equal(rsp[7], ST_OFFLINE);
    assert_false(ctlr.unit[1].present);
}

static void test_profile_supplies_mapped_buffer_tag(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    vax_mscp_profile profile = test_profile;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *dest;
    const uint32_t mapped_buffer_tag = 0x40000000u;
    const vax_mscp_bus bus = {
        .profile = &profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_map_pte = fake_read_any_map_pte,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    profile.buffer_map_bit = mapped_buffer_tag;
    profile.mapped_descriptor_max = mapped_buffer_tag - 1u;
    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 19u;
    mem.expected_sectors = 1u;
    mem.map_pte[0] = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(i ^ 0x3cu);

    assert_true(vax_mscp_init(&ctlr, &bus));
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0;
    cmd[11] = mapped_buffer_tag >> 16;
    cmd[12] = 0x0100u;
    cmd[13] = 0x0020u;
    cmd[16] = 19u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    dest = (uint8_t *)mem.words + (0x00200600u - mem.base);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(mem.map_reads, 1u);
    assert_memory_equal(dest, mem.sector, VAX_PAGE_BYTES);
}

static void test_get_unit_status_next_unit_wraps_to_zero(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_GUS);
    cmd[4] = 1;
    cmd[7] = GUS_NEXT_UNIT;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[4], 0);
    assert_int_equal(rsp[6], OP_GUS | OP_END);
    assert_int_equal(rsp[7], ST_AVAILABLE);
}

static void test_ip_poll_onlines_present_unit(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    (void)setup_single_command(&ctlr, &mem, OP_ONL);

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 44);
    assert_int_equal(rsp[4], 0);
    assert_int_equal(rsp[6], OP_ONL | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[ONL_MULTILUN_UNIT], 0);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000);
    assert_int_equal(rsp[MSCP_UNIT_UIDD], (2u << 8) | RA81_MODEL);
    assert_int_equal(rsp[MSCP_UNIT_SHADOW_UNIT], 0);
    assert_int_equal(rsp[MSCP_UNIT_SHADOW_STATUS], 0);
    assert_int_equal(rsp[20], 12345);
    assert_int_equal(rsp[21], 0);
}

static void test_ip_poll_online_already_online_preserves_flags(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].online = 1;
    ctlr.unit[0].compare_reads = 1;
    cmd = setup_single_command(&ctlr, &mem, OP_ONL);
    cmd[MSCP_UNIT_FLAGS] = UF_CMPWR;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_ONL | OP_END);
    assert_int_equal(rsp[7], ST_ALREADY_ONLINE);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000u | UF_CMPRD);
    assert_true(ctlr.unit[0].compare_reads);
    assert_false(ctlr.unit[0].compare_writes);
}

static void test_ip_poll_set_unit_characteristics_online_unit(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].online = 1;
    (void)setup_single_command(&ctlr, &mem, OP_SETUNITC);

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 44);
    assert_int_equal(rsp[6], OP_SETUNITC | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000);
    assert_int_equal(rsp[MSCP_UNIT_UIDD], (2u << 8) | RA81_MODEL);
    assert_int_equal(rsp[MSCP_UNIT_SHADOW_UNIT], 0);
    assert_int_equal(rsp[20], 12345);
    assert_int_equal(rsp[21], 0);
    assert_true(ctlr.unit[0].online);
}

static void test_ip_poll_set_unit_characteristics_available_unit(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    (void)setup_single_command(&ctlr, &mem, OP_SETUNITC);

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 44);
    assert_int_equal(rsp[6], OP_SETUNITC | OP_END);
    assert_int_equal(rsp[7], ST_AVAILABLE);
    assert_int_equal(rsp[MSCP_UNIT_UIDD], (2u << 8) | RA81_MODEL);
    assert_false(ctlr.unit[0].online);
}

static void
test_ip_poll_set_unit_characteristics_available_unit_ignores_flags(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_SETUNITC);
    cmd[7] = MD_STWRP;
    cmd[MSCP_UNIT_FLAGS] = UF_WRTPS;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_SETUNITC | OP_END);
    assert_int_equal(rsp[7], ST_AVAILABLE);
    assert_false(ctlr.unit[0].write_protected);
}

static void assert_onl_suc_rejects_reserved_word(uint16_t opcode,
                                                 uint16_t reserved_word,
                                                 uint16_t status)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, opcode);
    cmd[reserved_word] = 0x5a5au;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], status);
    assert_false(ctlr.unit[0].online);
}

static void test_ip_poll_online_rejects_reserved_word_6(void **state)
{
    (void)state;

    assert_onl_suc_rejects_reserved_word(OP_ONL, 8,
                                         ST_INVALID_ONL_RESERVED_FIELD);
}

static void test_ip_poll_online_rejects_shadow_reserved_words(void **state)
{
    (void)state;

    assert_onl_suc_rejects_reserved_word(OP_ONL, 18,
                                         ST_INVALID_SHADOW_RESERVED_FIELD);
    assert_onl_suc_rejects_reserved_word(OP_ONL, 19,
                                         ST_INVALID_SHADOW_RESERVED_FIELD);
}

static void
test_ip_poll_set_unit_characteristics_rejects_reserved_word_6(void **state)
{
    (void)state;

    assert_onl_suc_rejects_reserved_word(OP_SETUNITC, 8,
                                         ST_INVALID_ONL_RESERVED_FIELD);
}

static void test_ip_poll_suc_rejects_shadow_reserved_words(void **state)
{
    (void)state;

    assert_onl_suc_rejects_reserved_word(OP_SETUNITC, 18,
                                         ST_INVALID_SHADOW_RESERVED_FIELD);
    assert_onl_suc_rejects_reserved_word(OP_SETUNITC, 19,
                                         ST_INVALID_SHADOW_RESERVED_FIELD);
}

static void test_ip_poll_available_takes_online_unit_available(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].online = 1;
    (void)setup_single_command(&ctlr, &mem, OP_AVAILABLE);

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[4], 0);
    assert_int_equal(rsp[6], OP_AVAILABLE | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(ctlr.unit[0].online, 0);
}

static void test_ip_poll_read_available_unit_reports_available(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    cmd[8] = 512;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_AVAILABLE);
}

static void
test_ip_poll_read_zero_byte_count_reports_invalid_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = 0;
    cmd[16] = 1;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_INVALID_BYTE_COUNT);
    assert_int_equal(mem.sectors_read, 0);
}

static void
test_ip_poll_read_past_host_area_reports_invalid_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 10);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES * 2u;
    cmd[16] = 9;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_INVALID_BYTE_COUNT);
    assert_int_equal(mem.sectors_read, 0);
}

static void
test_ip_poll_read_outside_emulated_unit_reports_invalid_lbn(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 10);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = 1;
    cmd[16] = 10;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_INVALID_LBN);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_get_command_status_always_succeeds(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    cmd = setup_single_command(&ctlr, &mem, OP_GETCMDST);
    cmd[GCS_OUTSTANDING_REF_LOW] = 0x1122;
    cmd[GCS_OUTSTANDING_REF_LOW + 1] = 0x3344;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 20);
    assert_int_equal(rsp[6], OP_GETCMDST | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[GCS_OUTSTANDING_REF_LOW], 0x1122);
    assert_int_equal(rsp[GCS_OUTSTANDING_REF_LOW + 1], 0x3344);
    assert_int_equal(rsp[GCS_CMD_STATUS_LOW], 0);
    assert_int_equal(rsp[GCS_CMD_STATUS_LOW + 1], 0);
}

static void test_ip_poll_abort_always_succeeds(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ABORT);
    cmd[GCS_OUTSTANDING_REF_LOW] = 0x7788;
    cmd[GCS_OUTSTANDING_REF_LOW + 1] = 0x99aa;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);
    rsp = packet_at(&mem, RSP_PACKET);

    assert_int_equal(rsp[0], 16);
    assert_int_equal(rsp[6], OP_ABORT | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[GCS_OUTSTANDING_REF_LOW], 0x7788);
    assert_int_equal(rsp[GCS_OUTSTANDING_REF_LOW + 1], 0x99aa);
}

static void test_ip_poll_reserved_noop_opcodes_succeed(void **state)
{
    static const uint16_t opcodes[] = {OP_NOP17, OP_NOP19};

    (void)state;

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        FakeMemory mem;
        vax_mscp ctlr;
        uint16_t *rsp;
        const vax_mscp_bus bus = {
            .profile = &test_profile,
            .ctx = &mem,
            .read_words = fake_read_words,
            .write_words = fake_write_words,
            .write_bytes = fake_write_bytes,
            .read_sectors = fake_read_sectors,
            .ring_interrupt = fake_ring_interrupt,
        };

        memset(&mem, 0, sizeof(mem));
        mem.base = 0x00200000u - 4u;
        vax_mscp_init(&ctlr, &bus);
        bring_controller_up(&ctlr);

        rsp = run_simple_command(&mem, &ctlr, opcodes[i]);

        assert_int_equal(rsp[0], 12);
        assert_int_equal(rsp[6], opcodes[i] | OP_END);
        assert_int_equal(rsp[7], ST_SUCCESS);
    }
}

static void test_ip_poll_determine_access_paths_succeeds_as_noop(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);

    rsp = run_simple_command(&mem, &ctlr, OP_DAP);

    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_DAP | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
}

static void test_ip_poll_determine_access_paths_rejects_modifier(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    cmd = setup_single_command(&ctlr, &mem, OP_DAP);
    cmd[7] = MD_CSE;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MODIFIER);
}

static void test_ip_poll_display_without_mbi_succeeds_as_ignored(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);

    rsp = run_simple_command(&mem, &ctlr, OP_DISPLAY);

    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_DISPLAY | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
}

static void test_ip_poll_display_with_mbi_reports_invalid_item(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    cmd = setup_single_command(&ctlr, &mem, OP_DISPLAY);
    cmd[7] = MD_DISPLAY_MBI;
    cmd[8] = 1;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_DISPLAY | OP_END);
    assert_int_equal(rsp[7], ST_INVALID_ITEM);
}

static void test_ip_poll_display_rejects_invalid_modifier(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    cmd = setup_single_command(&ctlr, &mem, OP_DISPLAY);
    cmd[7] = MD_COMPARE;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MODIFIER);
}

static void
test_ip_poll_nontransfer_commands_reject_invalid_modifiers(void **state)
{
    static const uint16_t opcodes[] = {
        OP_ABORT,     OP_GETCMDST, OP_GUS,      OP_SCC,
        OP_AVAILABLE, OP_ONL,      OP_SETUNITC,
    };

    (void)state;

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        FakeMemory mem;
        vax_mscp ctlr;
        uint16_t *cmd;
        uint16_t *rsp;
        const vax_mscp_bus bus = {
            .profile = &test_profile,
            .ctx = &mem,
            .read_words = fake_read_words,
            .write_words = fake_write_words,
            .write_bytes = fake_write_bytes,
            .read_sectors = fake_read_sectors,
            .ring_interrupt = fake_ring_interrupt,
        };

        memset(&mem, 0, sizeof(mem));
        mem.base = 0x00200000u - 4u;
        vax_mscp_init(&ctlr, &bus);
        cmd = setup_single_command(&ctlr, &mem, opcodes[i]);
        cmd[7] = MD_COMPARE;

        assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

        rsp = packet_at(&mem, RSP_PACKET);
        assert_int_equal(rsp[0], 12);
        assert_int_equal(rsp[6], OP_END);
        assert_int_equal(rsp[7], ST_INVALID_MODIFIER);
    }
}

static void test_ip_poll_kdb50_rejects_bundled_shadowing_modifiers(void **state)
{
    static const uint16_t opcodes[] = {
        OP_AVAILABLE,
        OP_ONL,
        OP_SETUNITC,
    };

    (void)state;

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        FakeMemory mem;
        vax_mscp ctlr;
        uint16_t *cmd;
        uint16_t *rsp;
        const vax_mscp_bus bus = {
            .profile = &test_profile,
            .ctx = &mem,
            .read_words = fake_read_words,
            .write_words = fake_write_words,
            .write_bytes = fake_write_bytes,
            .read_sectors = fake_read_sectors,
            .ring_interrupt = fake_ring_interrupt,
        };

        memset(&mem, 0, sizeof(mem));
        mem.base = 0x00200000u - 4u;
        vax_mscp_init(&ctlr, &bus);
        vax_mscp_set_unit(&ctlr, 0, 12345);
        cmd = setup_single_command(&ctlr, &mem, opcodes[i]);
        cmd[7] = MD_SHADOW_UNIT;

        assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

        rsp = packet_at(&mem, RSP_PACKET);
        assert_int_equal(rsp[0], 12);
        assert_int_equal(rsp[6], OP_END);
        assert_int_equal(rsp[7], ST_INVALID_MODIFIER);
    }
}

static void test_ip_poll_unsupported_standard_opcodes_are_invalid(void **state)
{
    static const uint16_t opcodes[] = {
        OP_DCD, OP_FORMAT, OP_REPLACE, OP_WHM, OP_TERMINATE,
    };

    (void)state;

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        FakeMemory mem;
        vax_mscp ctlr;
        uint16_t *rsp;
        const vax_mscp_bus bus = {
            .profile = &test_profile,
            .ctx = &mem,
            .read_words = fake_read_words,
            .write_words = fake_write_words,
            .write_bytes = fake_write_bytes,
            .read_sectors = fake_read_sectors,
            .ring_interrupt = fake_ring_interrupt,
        };

        memset(&mem, 0, sizeof(mem));
        mem.base = 0x00200000u - 4u;
        vax_mscp_init(&ctlr, &bus);
        bring_controller_up(&ctlr);

        rsp = run_simple_command(&mem, &ctlr, opcodes[i]);

        assert_int_equal(rsp[0], 12);
        assert_int_equal(rsp[6], OP_END);
        assert_int_equal(rsp[7], ST_INVALID_OPCODE);
    }
}

static void
test_ip_poll_unknown_opcode_uses_invalid_command_endcode(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .ring_interrupt = fake_ring_interrupt,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    bring_controller_up(&ctlr);

    rsp = run_simple_command(&mem, &ctlr, 0x3fu);

    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_OPCODE);
}

static void test_ip_poll_rejects_generic_reserved_word(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 17;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[5] = 0x1234;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0600;
    cmd[11] = 0x0020;
    cmd[16] = 17;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_RESERVED_FIELD);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_reads_one_sector_to_physical_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 7;
    mem.expected_sectors = 1;
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)i;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = 512;
    cmd[10] = 0x0080;
    cmd[11] = 0x0020;
    cmd[16] = 7;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_read, 1);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200080u - mem.base),
                        mem.sector, VAX_PAGE_BYTES);
}

static void test_physical_read_uses_word_fallback_for_host_writes(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 11u;
    mem.expected_sectors = 1u;
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(0xa5u ^ i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0080;
    cmd[11] = 0x0020;
    cmd[16] = 11u;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200080u - mem.base),
                        mem.sector, VAX_PAGE_BYTES);
}

static void test_ip_poll_read_rejects_short_message(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 0;
    mem.expected_sectors = 1;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[0] = 30;
    cmd[8] = 1;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 12);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MESSAGE_LENGTH);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_reads_multiple_sectors_to_physical_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 9;
    mem.expected_sectors = 2;
    for (uint32_t i = 0; i < mem.expected_sectors * VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(255u - i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = mem.expected_sectors * VAX_PAGE_BYTES;
    cmd[10] = 0x0080;
    cmd[11] = 0x0020;
    cmd[16] = 9;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_read, 2);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200080u - mem.base),
                        mem.sector, mem.expected_sectors * VAX_PAGE_BYTES);
}

static void test_ip_poll_reads_partial_final_sector(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *target;
    const uint32_t byte_count = VAX_PAGE_BYTES + 188u;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 13;
    mem.expected_sectors = 2;
    for (uint32_t i = 0; i < mem.expected_sectors * VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(0x40u + i);
    target = ((uint8_t *)mem.words) + (0x00200400u - mem.base);
    memset(target, 0xa5, VAX_PAGE_BYTES * 2u);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = byte_count;
    cmd[10] = 0x0400;
    cmd[11] = 0x0020;
    cmd[16] = 13;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[8], byte_count);
    assert_int_equal(mem.sectors_read, 2);
    assert_memory_equal(target, mem.sector, byte_count);
    assert_int_equal(target[byte_count], 0xa5);
}

static void test_ip_poll_reads_through_kdb_pte_map(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint32_t pte = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);
    uint32_t pte2 = VAX_PTE_VALID | ((0x00200800u >> 9) & VAX_PTE_PFN_MASK);
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 7;
    mem.expected_sectors = 1;
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(i ^ 0x5au);

    write_desc(&mem, 0x00200100u, pte);
    write_desc(&mem, 0x00200104u, pte2);
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0034;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x0100;
    cmd[13] = 0x0020;
    cmd[16] = 7;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_read, 1);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200634u - mem.base),
                        mem.sector, VAX_PAGE_BYTES - 0x34u);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200800u - mem.base),
                        &mem.sector[VAX_PAGE_BYTES - 0x34u], 0x34u);
}

static void test_ip_poll_read_rejects_odd_mapped_bi_address(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .read_map_pte = fake_read_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 14;
    mem.expected_sectors = 1;
    mem.map_pte[0] = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = 2;
    cmd[10] = 0x0001;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x01f8;
    cmd[13] = 0x0020;
    cmd[16] = 14;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_ADDRESS);
    assert_int_equal(rsp[8], 0);
}

static void test_ip_poll_read_rejects_odd_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 15;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = 1;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 15;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_BYTE_COUNT);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_read_rejects_mapped_mapbase_mbz_bits(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .read_map_pte = fake_read_any_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 18;
    mem.expected_sectors = 1;
    mem.map_pte[0] = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x0100;
    cmd[13] = 0x4020;
    cmd[16] = 18;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ACCESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.map_reads, 0);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_read_rejects_mapped_pte_address_overflow(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words_fails_high_addr,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 19;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0xfe00;
    cmd[11] = 0xffff;
    cmd[12] = 0xfffc;
    cmd[13] = 0x3fff;
    cmd[16] = 19;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ACCESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.map_reads, 0);
    assert_int_equal(mem.sectors_read, 0);
}

static void
test_ip_poll_reads_through_pte_callback_after_page_boundary(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .read_map_pte = fake_read_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 15;
    mem.expected_sectors = 3;
    for (uint32_t i = 0; i < mem.expected_sectors; i++)
        mem.map_pte[i] =
            VAX_PTE_VALID |
            (((0x00200600u + (i * VAX_PAGE_BYTES)) >> 9) & VAX_PTE_PFN_MASK);
    for (uint32_t i = 0; i < mem.expected_sectors * VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(i ^ 0xa5u);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = mem.expected_sectors * VAX_PAGE_BYTES;
    cmd[10] = 0;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x01f8;
    cmd[13] = 0x0020;
    cmd[16] = 15;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_read, 3);
    assert_memory_equal(((uint8_t *)mem.words) + (0x00200600u - mem.base),
                        mem.sector, mem.expected_sectors * VAX_PAGE_BYTES);
}

static void
test_ip_poll_read_compare_reports_corrupt_host_transfer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 18;
    mem.expected_sectors = 1;
    mem.corrupt_host_writes = 1;
    mem.repeat_reads = 1;
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(0x55u + i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[7] = MD_COMPARE;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 18;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_COMPARE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 2);
}

static void test_ip_poll_compare_read_unit_flag_verifies_reads(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 18;
    mem.expected_sectors = 1;
    mem.corrupt_host_writes = 1;
    mem.repeat_reads = 1;
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        mem.sector[i] = (uint8_t)(0x55u + i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    bring_controller_up_with_two_entry_rings(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | RSP_PACKET2);
    write_desc(&mem, 0x00200008u, UQ_OWN | CMD_PACKET);
    write_desc(&mem, 0x0020000Cu, UQ_OWN | CMD_PACKET2);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_ONL;
    cmd[MSCP_UNIT_FLAGS] = UF_CMPRD;

    cmd = command_packet_at(&mem, CMD_PACKET2);
    cmd[6] = OP_RD;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 18;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_ONL | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000u | UF_CMPRD);

    rsp = packet_at(&mem, RSP_PACKET2);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_COMPARE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 2);
}

static void
test_ip_poll_read_host_error_reports_completed_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .read_map_pte = fake_read_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 20;
    mem.expected_sectors = 3;
    mem.map_pte[0] = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);
    mem.map_pte[1] = VAX_PTE_VALID | ((0x00200800u >> 9) & VAX_PTE_PFN_MASK);
    mem.map_pte[2] = 0;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = mem.expected_sectors * VAX_PAGE_BYTES;
    cmd[10] = 0;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x01f8;
    cmd[13] = 0x0020;
    cmd[16] = 20;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_INVALID_PTE);
    assert_int_equal(rsp[8], 2u * VAX_PAGE_BYTES);
    assert_int_equal(rsp[9], 0);
}

static void
test_ip_poll_read_reports_noncontiguous_kdb_pte_list(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const uint32_t byte_count = 0x0000e000u;
    const uint32_t failing_page = 0x61u;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .read_map_pte = fake_read_any_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 64;
    mem.expected_sectors = byte_count / VAX_PAGE_BYTES;
    for (uint32_t i = 0; i < failing_page; i++) {
        mem.map_pte[i] =
            VAX_PTE_VALID |
            (((0x00200600u + (i * VAX_PAGE_BYTES)) >> 9) & VAX_PTE_PFN_MASK);
    }
    mem.map_pte[failing_page] = 0;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = byte_count;
    cmd[10] = 0;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x3280;
    cmd[13] = 0x00a6;
    cmd[16] = 64;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_INVALID_PTE);
    assert_int_equal(rsp[8], failing_page * VAX_PAGE_BYTES);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(mem.sectors_read, failing_page + 1u);
    assert_int_equal(mem.map_reads, failing_page + 1u);
}

static void test_ip_poll_read_rejects_invalid_unmapped_bi_address(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *alias;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 21;
    mem.expected_sectors = 1;
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);
    alias = ((uint8_t *)mem.words) + (0x00200600u - mem.base);
    memset(alias, 0xa5, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0600;
    cmd[11] = 0x4020;
    cmd[16] = 21;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ACCESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(alias[0], 0xa5);
}

static void
test_ip_poll_access_reads_disk_with_reserved_buffer_zero(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 23;
    mem.expected_sectors = 1;
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ACC);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[16] = 23;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_ACC | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(rsp[8], VAX_PAGE_BYTES);
    assert_int_equal(mem.sectors_read, 1);
}

static void test_ip_poll_access_rejects_reserved_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 24;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ACC);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0600;
    cmd[16] = 24;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_BUFFER_RESERVED_FIELD);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_access_rejects_odd_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 24;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ACC);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES + 1u;
    cmd[16] = 24;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_ACC | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_BYTE_COUNT);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(mem.sectors_read, 0);
}

static void test_ip_poll_compare_succeeds_when_host_data_matches(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 24;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0x70u + i);
    memcpy(mem.sector, source, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_CMP);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 24;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_CMP | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(mem.sectors_read, 1);
}

static void test_ip_poll_compare_reports_mismatched_host_data(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 25;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    memset(source, 0x11, VAX_PAGE_BYTES);
    memset(mem.sector, 0x11, VAX_PAGE_BYTES);
    mem.sector[137] = 0x22;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_CMP);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 25;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_CMP | OP_END);
    assert_int_equal(rsp[7], ST_COMPARE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 1);
}

static void test_ip_poll_writes_one_sector_from_physical_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 11;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0x80u ^ i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_written, 1);
    assert_memory_equal(mem.sector, source, VAX_PAGE_BYTES);
}

static void test_ip_poll_write_rejects_invalid_unmapped_bi_address(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *alias;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 22;
    mem.expected_sectors = 1;
    alias = ((uint8_t *)mem.words) + (0x00200600u - mem.base);
    memset(alias, 0x33, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0600;
    cmd[11] = 0x4020;
    cmd[16] = 22;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ACCESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void test_ip_poll_write_rejects_odd_unmapped_bi_address(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 22;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    memset(source, 0x33, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0481;
    cmd[11] = 0x0020;
    cmd[16] = 22;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_ADDRESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void test_ip_poll_write_rejects_odd_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_bytes = fake_read_bytes,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 16;
    mem.expected_sectors = 1;

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = 1;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 16;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_BYTE_COUNT);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void test_ip_poll_write_rejects_unaligned_mapped_mapbase(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_bytes = fake_read_bytes,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
        .read_map_pte = fake_read_any_map_pte,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 19;
    mem.expected_sectors = 1;
    mem.map_pte[0] = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x0102;
    cmd[13] = 0x0020;
    cmd[16] = 19;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ACCESS);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.map_reads, 0);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void test_ip_poll_force_error_write_marks_later_read(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    uint8_t *dest;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 11;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    dest = ((uint8_t *)mem.words) + (0x00200680u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0x40u ^ i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[7] = MD_FORCE_ERROR;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_memory_equal(mem.sector, source, VAX_PAGE_BYTES);

    mem.sectors_read = 0;
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0680;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_DATA_ERROR_FORCE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_memory_equal(dest, source, VAX_PAGE_BYTES);
}

static void test_ip_poll_normal_write_clears_force_error(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    uint8_t *dest;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 11;
    mem.expected_sectors = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    dest = ((uint8_t *)mem.words) + (0x00200680u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0x55u + i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].online = 1;
    ctlr.unit[0].forced_error[11u >> 3] = (uint8_t)(1u << (11u & 7u));
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);

    mem.sectors_read = 0;
    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0680;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_memory_equal(dest, source, VAX_PAGE_BYTES);
}

static void test_set_unit_preserves_force_error_for_same_media(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *dest;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 11;
    mem.expected_sectors = 1;
    dest = ((uint8_t *)mem.words) + (0x00200680u - mem.base);
    memset(mem.sector, 0x6a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].forced_error[11u >> 3] = (uint8_t)(1u << (11u & 7u));
    vax_mscp_set_unit(&ctlr, 0, 12345);

    cmd = setup_single_command(&ctlr, &mem, OP_RD);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0680;
    cmd[11] = 0x0020;
    cmd[16] = 11;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_RD | OP_END);
    assert_int_equal(rsp[7], ST_DATA_ERROR_FORCE_ERROR);
    assert_memory_equal(dest, mem.sector, VAX_PAGE_BYTES);
}

static void
test_ip_poll_write_rejects_software_write_protected_unit(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);

    cmd = setup_single_command(&ctlr, &mem, OP_ONL);
    cmd[7] = MD_STWRP;
    cmd[MSCP_UNIT_FLAGS] = UF_WRTPS;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_ONL | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000u | UF_WRTPS);

    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    cmd[8] = VAX_PAGE_BYTES;
    cmd[16] = 4;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_WRITE_PROTECTED_SOFTWARE);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void
test_ip_poll_write_history_modifier_reports_invalid_modifier(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[7] = 0x0008;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[16] = 4;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_MODIFIER);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
}

static void test_ip_poll_erase_writes_zeroes_without_host_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 26;
    mem.expected_sectors = 1;
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ERS);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[16] = 26;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_ERS | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(mem.sectors_read, 1);
    assert_int_equal(mem.sectors_written, 1);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        assert_int_equal(mem.sector[i], 0);
}

static void test_ip_poll_erase_rejects_reserved_buffer(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 27;
    mem.expected_sectors = 1;
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ERS);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0600;
    cmd[16] = 27;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_END);
    assert_int_equal(rsp[7], ST_INVALID_BUFFER_RESERVED_FIELD);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
    assert_int_equal(mem.sector[0], 0x5a);
}

static void test_ip_poll_erase_rejects_odd_byte_count(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .write_words = fake_write_words,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 27;
    mem.expected_sectors = 1;
    memset(mem.sector, 0x5a, VAX_PAGE_BYTES);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_ERS);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES + 1u;
    cmd[16] = 27;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_ERS | OP_END);
    assert_int_equal(rsp[7], ST_HOST_BUFFER_ODD_BYTE_COUNT);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(rsp[9], 0);
    assert_int_equal(mem.sectors_read, 0);
    assert_int_equal(mem.sectors_written, 0);
    assert_int_equal(mem.sector[0], 0x5a);
}

static void test_ip_poll_write_compare_reports_corrupt_media_write(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 27;
    mem.expected_sectors = 1;
    mem.corrupt_disk_writes = 1;
    mem.repeat_reads = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0xa0u + i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[7] = MD_COMPARE;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 27;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_COMPARE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 2);
    assert_int_equal(mem.sectors_written, 1);
}

static void test_ip_poll_compare_write_unit_flag_verifies_writes(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 27;
    mem.expected_sectors = 1;
    mem.corrupt_disk_writes = 1;
    mem.repeat_reads = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0xa0u + i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    ctlr.unit[0].online = 1;
    bring_controller_up_with_two_entry_rings(&ctlr);
    write_desc(&mem, 0x00200000u, UQ_OWN | RSP_PACKET);
    write_desc(&mem, 0x00200004u, UQ_OWN | RSP_PACKET2);
    write_desc(&mem, 0x00200008u, UQ_OWN | CMD_PACKET);
    write_desc(&mem, 0x0020000Cu, UQ_OWN | CMD_PACKET2);

    cmd = command_packet_at(&mem, CMD_PACKET);
    cmd[6] = OP_SETUNITC;
    cmd[MSCP_UNIT_FLAGS] = UF_CMPWR;

    cmd = command_packet_at(&mem, CMD_PACKET2);
    cmd[6] = OP_WR;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 27;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_SETUNITC | OP_END);
    assert_int_equal(rsp[7], ST_SUCCESS);
    assert_int_equal(rsp[MSCP_UNIT_FLAGS], 0x8000u | UF_CMPWR);

    rsp = packet_at(&mem, RSP_PACKET2);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_COMPARE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 2);
    assert_int_equal(mem.sectors_written, 1);
}

static void
test_ip_poll_force_error_write_compare_reports_data_error(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 28;
    mem.expected_sectors = 1;
    mem.repeat_reads = 1;
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        source[i] = (uint8_t)(0x70u ^ i);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[7] = MD_FORCE_ERROR | MD_COMPARE;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 28;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], ST_DATA_ERROR_FORCE_ERROR);
    assert_int_equal(rsp[8], 0);
    assert_int_equal(mem.sectors_read, 2);
    assert_int_equal(mem.sectors_written, 1);
}

static void test_ip_poll_writes_through_kdb_pte_map(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t expected[VAX_PAGE_BYTES];
    uint8_t *first;
    uint8_t *second;
    uint32_t pte = VAX_PTE_VALID | ((0x00200600u >> 9) & VAX_PTE_PFN_MASK);
    uint32_t pte2 = VAX_PTE_VALID | ((0x00200800u >> 9) & VAX_PTE_PFN_MASK);
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 12;
    mem.expected_sectors = 1;
    first = ((uint8_t *)mem.words) + (0x00200634u - mem.base);
    second = ((uint8_t *)mem.words) + (0x00200800u - mem.base);
    for (uint32_t i = 0; i < VAX_PAGE_BYTES; i++)
        expected[i] = (uint8_t)(0x33u + i);
    memcpy(first, expected, VAX_PAGE_BYTES - 0x34u);
    memcpy(second, &expected[VAX_PAGE_BYTES - 0x34u], 0x34u);

    write_desc(&mem, 0x00200100u, pte);
    write_desc(&mem, 0x00200104u, pte2);
    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = VAX_PAGE_BYTES;
    cmd[10] = 0x0034;
    cmd[11] = KDB_MAP >> 16;
    cmd[12] = 0x0100;
    cmd[13] = 0x0020;
    cmd[16] = 12;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[0], 32);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(mem.sectors_written, 1);
    assert_memory_equal(mem.sector, expected, VAX_PAGE_BYTES);
}

static void test_ip_poll_writes_partial_final_sector(void **state)
{
    FakeMemory mem;
    vax_mscp ctlr;
    uint16_t *cmd;
    uint16_t *rsp;
    uint8_t *source;
    uint8_t expected[VAX_PAGE_BYTES * 2u];
    const uint32_t byte_count = VAX_PAGE_BYTES + 188u;
    const vax_mscp_bus bus = {
        .profile = &test_profile,
        .ctx = &mem,
        .read_words = fake_read_words,
        .read_bytes = fake_read_bytes,
        .write_words = fake_write_words,
        .write_bytes = fake_write_bytes,
        .read_sectors = fake_read_sectors,
        .write_sectors = fake_write_sectors,
    };

    (void)state;

    memset(&mem, 0, sizeof(mem));
    mem.base = 0x00200000u - 4u;
    mem.expected_unit = 0;
    mem.expected_lbn = 17;
    mem.expected_sectors = 2;
    for (uint32_t i = 0; i < sizeof(expected); i++)
        expected[i] = (uint8_t)(0x20u + i);
    memcpy(mem.sector, expected, sizeof(expected));
    source = ((uint8_t *)mem.words) + (0x00200480u - mem.base);
    for (uint32_t i = 0; i < byte_count; i++)
        source[i] = (uint8_t)(0x90u ^ i);
    memcpy(expected, source, byte_count);

    vax_mscp_init(&ctlr, &bus);
    vax_mscp_set_unit(&ctlr, 0, 12345);
    cmd = setup_single_command(&ctlr, &mem, OP_WR);
    ctlr.unit[0].online = 1;
    cmd[8] = byte_count;
    cmd[10] = 0x0480;
    cmd[11] = 0x0020;
    cmd[16] = 17;

    assert_int_equal(vax_mscp_read_ip(&ctlr), 0);

    rsp = packet_at(&mem, RSP_PACKET);
    assert_int_equal(rsp[6], OP_WR | OP_END);
    assert_int_equal(rsp[7], 0);
    assert_int_equal(rsp[8], byte_count);
    assert_int_equal(mem.sectors_read, 2);
    assert_int_equal(mem.sectors_written, 2);
    assert_memory_equal(mem.sector, expected, sizeof(expected));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_uses_callback_memory_for_comm_region),
        cmocka_unit_test(test_init_clears_entire_comm_region_for_large_rings),
        cmocka_unit_test(test_diagnostic_wrap_echoes_sa_until_reset),
        cmocka_unit_test(
            test_purge_poll_reaches_step4_after_zero_sa_and_ip_read),
        cmocka_unit_test(test_purge_poll_nonzero_sa_reports_purge_poll_failure),
        cmocka_unit_test(test_init_reports_fatal_when_comm_region_write_fails),
        cmocka_unit_test(test_last_fail_survives_until_lf_initialization),
        cmocka_unit_test(test_last_fail_waits_for_response_buffer),
        cmocka_unit_test(test_init_step3_echoes_ie_and_vector_from_step1),
        cmocka_unit_test(test_init_interrupts_at_steps_1_to_3),
        cmocka_unit_test(test_step4_go_zero_ignores_all_other_host_options),
        cmocka_unit_test(test_step4_unsupported_sfm_enters_normal_operation),
        cmocka_unit_test(test_step4_ignores_node_name_when_not_advertised),
        cmocka_unit_test(test_ip_poll_rejects_nonsequential_command_packet),
        cmocka_unit_test(test_ip_poll_rejects_non_disk_mscp_connection_id),
        cmocka_unit_test(test_ip_poll_reports_queue_read_failure),
        cmocka_unit_test(test_ip_poll_reports_packet_read_failure),
        cmocka_unit_test(test_ip_poll_reports_packet_write_failure),
        cmocka_unit_test(test_ip_poll_reports_command_interrupt_write_failure),
        cmocka_unit_test(test_ip_poll_reports_response_interrupt_write_failure),
        cmocka_unit_test(test_zero_vector_suppresses_port_interrupts),
        cmocka_unit_test(test_ip_poll_completes_set_controller_characteristics),
        cmocka_unit_test(
            test_ip_poll_set_controller_characteristics_rejects_mscp_version),
        cmocka_unit_test(
            test_ip_poll_set_controller_characteristics_rejects_short_message),
        cmocka_unit_test(
            test_ip_poll_set_controller_characteristics_ignores_optional_flags),
        cmocka_unit_test(
            test_ip_poll_suppresses_response_interrupt_without_flag),
        cmocka_unit_test(
            test_ip_poll_posts_command_transition_when_ring_was_full),
        cmocka_unit_test(test_ip_poll_posts_only_first_response_transition),
        cmocka_unit_test(test_ip_poll_preserves_high_descriptor_addresses),
        cmocka_unit_test(test_init_ignores_vax_pte_bits_in_ring_base),
        cmocka_unit_test(test_ip_poll_ignores_vax_pte_bits_in_descriptors),
        cmocka_unit_test(test_ip_poll_drains_available_command_ring_entries),
        cmocka_unit_test(test_profile_validation_rejects_bad_shapes),
        cmocka_unit_test(test_init_and_reset_reject_invalid_profiles),
        cmocka_unit_test(test_reset_with_bus_preserves_units_and_last_fail),
        cmocka_unit_test(test_ip_poll_reports_present_unit_status),
        cmocka_unit_test(test_profile_supplies_controller_and_unit_identity),
        cmocka_unit_test(test_profile_limits_visible_units),
        cmocka_unit_test(test_profile_supplies_mapped_buffer_tag),
        cmocka_unit_test(test_get_unit_status_next_unit_wraps_to_zero),
        cmocka_unit_test(test_ip_poll_onlines_present_unit),
        cmocka_unit_test(test_ip_poll_online_already_online_preserves_flags),
        cmocka_unit_test(test_ip_poll_set_unit_characteristics_online_unit),
        cmocka_unit_test(test_ip_poll_set_unit_characteristics_available_unit),
        cmocka_unit_test(
            test_ip_poll_set_unit_characteristics_available_unit_ignores_flags),
        cmocka_unit_test(test_ip_poll_online_rejects_reserved_word_6),
        cmocka_unit_test(test_ip_poll_online_rejects_shadow_reserved_words),
        cmocka_unit_test(
            test_ip_poll_set_unit_characteristics_rejects_reserved_word_6),
        cmocka_unit_test(test_ip_poll_suc_rejects_shadow_reserved_words),
        cmocka_unit_test(test_ip_poll_available_takes_online_unit_available),
        cmocka_unit_test(test_ip_poll_read_available_unit_reports_available),
        cmocka_unit_test(
            test_ip_poll_read_zero_byte_count_reports_invalid_byte_count),
        cmocka_unit_test(
            test_ip_poll_read_past_host_area_reports_invalid_byte_count),
        cmocka_unit_test(
            test_ip_poll_read_outside_emulated_unit_reports_invalid_lbn),
        cmocka_unit_test(test_ip_poll_get_command_status_always_succeeds),
        cmocka_unit_test(test_ip_poll_abort_always_succeeds),
        cmocka_unit_test(test_ip_poll_reserved_noop_opcodes_succeed),
        cmocka_unit_test(test_ip_poll_determine_access_paths_succeeds_as_noop),
        cmocka_unit_test(test_ip_poll_determine_access_paths_rejects_modifier),
        cmocka_unit_test(test_ip_poll_display_without_mbi_succeeds_as_ignored),
        cmocka_unit_test(test_ip_poll_display_with_mbi_reports_invalid_item),
        cmocka_unit_test(test_ip_poll_display_rejects_invalid_modifier),
        cmocka_unit_test(
            test_ip_poll_nontransfer_commands_reject_invalid_modifiers),
        cmocka_unit_test(
            test_ip_poll_kdb50_rejects_bundled_shadowing_modifiers),
        cmocka_unit_test(test_ip_poll_unsupported_standard_opcodes_are_invalid),
        cmocka_unit_test(
            test_ip_poll_unknown_opcode_uses_invalid_command_endcode),
        cmocka_unit_test(test_ip_poll_rejects_generic_reserved_word),
        cmocka_unit_test(test_ip_poll_reads_one_sector_to_physical_buffer),
        cmocka_unit_test(test_physical_read_uses_word_fallback_for_host_writes),
        cmocka_unit_test(test_ip_poll_read_rejects_short_message),
        cmocka_unit_test(
            test_ip_poll_reads_multiple_sectors_to_physical_buffer),
        cmocka_unit_test(test_ip_poll_reads_partial_final_sector),
        cmocka_unit_test(test_ip_poll_reads_through_kdb_pte_map),
        cmocka_unit_test(test_ip_poll_read_rejects_odd_mapped_bi_address),
        cmocka_unit_test(test_ip_poll_read_rejects_odd_byte_count),
        cmocka_unit_test(test_ip_poll_read_rejects_mapped_mapbase_mbz_bits),
        cmocka_unit_test(test_ip_poll_read_rejects_mapped_pte_address_overflow),
        cmocka_unit_test(
            test_ip_poll_reads_through_pte_callback_after_page_boundary),
        cmocka_unit_test(
            test_ip_poll_read_compare_reports_corrupt_host_transfer),
        cmocka_unit_test(test_ip_poll_compare_read_unit_flag_verifies_reads),
        cmocka_unit_test(
            test_ip_poll_read_host_error_reports_completed_byte_count),
        cmocka_unit_test(
            test_ip_poll_read_reports_noncontiguous_kdb_pte_list),
        cmocka_unit_test(test_ip_poll_read_rejects_invalid_unmapped_bi_address),
        cmocka_unit_test(
            test_ip_poll_access_reads_disk_with_reserved_buffer_zero),
        cmocka_unit_test(test_ip_poll_access_rejects_reserved_buffer),
        cmocka_unit_test(test_ip_poll_access_rejects_odd_byte_count),
        cmocka_unit_test(test_ip_poll_compare_succeeds_when_host_data_matches),
        cmocka_unit_test(test_ip_poll_compare_reports_mismatched_host_data),
        cmocka_unit_test(test_ip_poll_writes_one_sector_from_physical_buffer),
        cmocka_unit_test(
            test_ip_poll_write_rejects_invalid_unmapped_bi_address),
        cmocka_unit_test(test_ip_poll_write_rejects_odd_unmapped_bi_address),
        cmocka_unit_test(test_ip_poll_write_rejects_odd_byte_count),
        cmocka_unit_test(test_ip_poll_write_rejects_unaligned_mapped_mapbase),
        cmocka_unit_test(test_ip_poll_force_error_write_marks_later_read),
        cmocka_unit_test(test_ip_poll_normal_write_clears_force_error),
        cmocka_unit_test(test_set_unit_preserves_force_error_for_same_media),
        cmocka_unit_test(
            test_ip_poll_write_rejects_software_write_protected_unit),
        cmocka_unit_test(
            test_ip_poll_write_history_modifier_reports_invalid_modifier),
        cmocka_unit_test(test_ip_poll_erase_writes_zeroes_without_host_buffer),
        cmocka_unit_test(test_ip_poll_erase_rejects_reserved_buffer),
        cmocka_unit_test(test_ip_poll_erase_rejects_odd_byte_count),
        cmocka_unit_test(
            test_ip_poll_write_compare_reports_corrupt_media_write),
        cmocka_unit_test(test_ip_poll_compare_write_unit_flag_verifies_writes),
        cmocka_unit_test(
            test_ip_poll_force_error_write_compare_reports_data_error),
        cmocka_unit_test(test_ip_poll_writes_through_kdb_pte_map),
        cmocka_unit_test(test_ip_poll_writes_partial_final_sector),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
