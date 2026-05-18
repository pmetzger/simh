/* cmdvars_env_helper.c: child-process environment capture helper */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *value;
    FILE *file;
    size_t len;

    if (argc != 3)
        return 2;

    value = getenv(argv[1]);
    if (value == NULL)
        return 3;

    file = fopen(argv[2], "wb");
    if (file == NULL)
        return 4;

    len = strlen(value);
    if (fwrite(value, 1, len, file) != len) {
        fclose(file);
        return 5;
    }

    return fclose(file) == 0 ? 0 : 6;
}
