#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "test_simh_personality.h"
#include "test_support.h"

struct simh_test_message_context {
    const char *payload;
    t_stat status;
};

static int setup_scp_message_fixture(void **state)
{
    (void)state;

    simh_test_reset_simulator_state();
    sim_switches = 0;
    sim_quiet = 0;
    sim_show_message = 1;
    sim_is_running = false;
    sim_log = NULL;
    sim_deb = NULL;
    return 0;
}

static int teardown_scp_message_fixture(void **state)
{
    (void)state;

    simh_test_reset_simulator_state();
    sim_switches = 0;
    sim_quiet = 0;
    sim_show_message = 1;
    sim_is_running = false;
    sim_log = NULL;
    sim_deb = NULL;
    return 0;
}

static void write_long_message(void *context)
{
    struct simh_test_message_context *message_context = context;

    message_context->status =
        sim_messagef(SCPE_ARG, "%s", message_context->payload);
}

static void sim_messagef_prints_messages_larger_than_stack_buffer(void **state)
{
    enum { PAYLOAD_SIZE = 4096 };
    static const char prefix[] = "%SIM-ERROR: ";
    char *payload;
    char *text = NULL;
    size_t text_size = 0;
    struct simh_test_message_context context;

    (void)state;

    payload = malloc(PAYLOAD_SIZE + 1);
    assert_non_null(payload);
    memset(payload, 'M', PAYLOAD_SIZE);
    payload[PAYLOAD_SIZE] = '\0';
    context.payload = payload;
    context.status = SCPE_OK;

    assert_int_equal(simh_test_capture_stdout(write_long_message, &context,
                                              &text, &text_size),
                     0);
    assert_int_equal(context.status & ~SCPE_NOMESSAGE, SCPE_ARG);
    assert_memory_equal(text, prefix, strlen(prefix));
    assert_memory_equal(text + strlen(prefix), payload, PAYLOAD_SIZE);
    assert_int_equal(text_size, strlen(prefix) + PAYLOAD_SIZE);

    free(text);
    free(payload);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            sim_messagef_prints_messages_larger_than_stack_buffer,
            setup_scp_message_fixture, teardown_scp_message_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
