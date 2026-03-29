#include <stdio.h>
#include <string.h>

void secret(void) {
    printf("*** You hijacked control flow! ***\n");
}

void vulnerable(char *input) {
    char buffer[64];
    printf("buffer @ %p\n", buffer);
    strcpy(buffer, input);  /* no bounds check */
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input>\n", argv[0]);
        return 1;
    }
    vulnerable(argv[1]);
    printf("Returned normally.\n");
    return 0;
}
