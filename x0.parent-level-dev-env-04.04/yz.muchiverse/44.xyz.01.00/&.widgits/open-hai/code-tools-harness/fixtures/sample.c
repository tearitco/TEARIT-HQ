#include <stdio.h>

/* sample fixture for the native-tools probe (code read/edit/exec).
 * Deliberate off-by-one bug: loop stops at N-1, so it prints 10
 * instead of 15. The edit scenario must change `i < N` to `i <= N`. */
#define N 5

int main(void) {
    int sum = 0;
    for (int i = 1; i < N; i++) sum += i; /* bug: should be i <= N */
    printf("sum(1..%d) = %d\n", N, sum);
    return 0;
}
