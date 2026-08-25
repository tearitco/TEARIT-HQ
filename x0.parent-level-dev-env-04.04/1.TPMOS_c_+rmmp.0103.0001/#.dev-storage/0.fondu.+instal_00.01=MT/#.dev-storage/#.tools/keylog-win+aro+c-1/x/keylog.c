#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int ch;

    printf("Enter text (Ctrl+C to exit):\n");

    while (1) {
        ch = getchar();
        if (ch != EOF) {
            printf("'%c' -> ASCII: %d\n", ch, ch);
        }
    }

    return 0;
}
