#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Record {
    char name[64];
    void (*action)(void);
};

void safe_action(void) {
    printf("Safe action executed.\n");
}

void win(void) {
    printf("*** You hijacked the function pointer! ***\n");
}

int main(int argc, char *argv[]) {
    struct Record *rec;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <name>\n", argv[0]);
        return 1;
    }

    rec = malloc(sizeof(struct Record));
    rec->action = safe_action;

    printf("rec @ %p, rec->action @ %p\n", rec, &rec->action);
    printf("win() @ %p\n", win);

    strcpy(rec->name, argv[1]);  /* heap overflow into function pointer */

    rec->action();
    free(rec);
    return 0;
}
