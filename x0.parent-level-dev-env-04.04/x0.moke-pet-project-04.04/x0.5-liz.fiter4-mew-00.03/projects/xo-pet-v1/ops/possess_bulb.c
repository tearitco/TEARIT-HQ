#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // For PoC, we communicate with the manager via a state file or just assume it reads the update
    // Actually, let's have the manager read a "target_possess.txt" file or similar.
    // Or just have the manager be the one to handle these specifically in route_input.
    // But to be TPMOS compliant, it should be an Op.
    
    FILE *f = fopen("pieces/manager/active_target.txt", "w");
    if (f) {
        fprintf(f, "liz_bulb\n");
        fclose(f);
    }
    printf("Possessing liz_bulb...");
    return 0;
}
