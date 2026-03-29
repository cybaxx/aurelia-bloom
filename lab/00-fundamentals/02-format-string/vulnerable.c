#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int auth = 0;

void check_auth(void) {
    if (auth) {
        printf("*** Access granted! auth = %d ***\n", auth);
    }
}

int main(int argc, char *argv[]) {
    char buffer[256];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <format_string>\n", argv[0]);
        return 1;
    }

    printf("auth @ %p\n", &auth);

    snprintf(buffer, sizeof(buffer), argv[1]);  /* format string bug */
    printf("%s\n", buffer);

    check_auth();
    return 0;
}
