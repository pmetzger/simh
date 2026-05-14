#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "scp.h"
#include "test_simh_personality.h"
#include "test_support.h"

/* Verify the backend name/version helpers report the required backend. */
static void test_sim_regex_backend_helpers_match_build_configuration(void **state)
{
    const char *backend;
    const char *version;

    (void)state;

    backend = sim_regex_backend_name();
    version = sim_regex_backend_version();

    assert_string_equal(backend, "PCRE2");
    assert_non_null(version);
    assert_true(version[0] != '\0');
}

/* Verify the regex support formatter describes the current build mode. */
static void test_sim_fprint_regex_support_reports_current_backend(void **state)
{
    const char *backend;
    const char *version;
    FILE *stream;
    char *text;
    size_t size;

    (void)state;

    backend = sim_regex_backend_name();
    version = sim_regex_backend_version();
    stream = sim_tmpfile();
    assert_non_null(stream);

    sim_fprint_regex_support(stream);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(text);

    assert_non_null(strstr(text, backend));
    assert_non_null(strstr(text, "EXPECT commands"));
    if (version && *version)
        assert_non_null(strstr(text, version));

    free(text);
    fclose(stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_sim_regex_backend_helpers_match_build_configuration),
        cmocka_unit_test(
            test_sim_fprint_regex_support_reports_current_backend),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
