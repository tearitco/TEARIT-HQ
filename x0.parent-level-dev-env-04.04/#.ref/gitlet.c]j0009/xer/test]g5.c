#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    system("mkdir temp_test");
    chdir("temp_test");

    printf("--- Testing init ---\n");
    system("../gitlet init");

    printf("\n--- Testing add ---\n");
    system("echo 'hello' > test.txt");
    system("../gitlet add test.txt");

    printf("\n--- Testing commit ---\n");
    system("../gitlet commit \"first commit\"");

    printf("\n--- Testing log ---\n");
    system("../gitlet log");

    printf("\n--- Testing status ---\n");
    system("../gitlet status");

    printf("\n--- Testing branch ---\n");
    system("../gitlet branch new-branch");
    system("../gitlet branch");

    printf("\n--- Testing checkout ---\n");
    system("../gitlet checkout new-branch");
    system("cat .gitlet/HEAD");

    printf("\n--- Testing rm ---\n");
    system("../gitlet rm test.txt");
    system("../gitlet status");
    system("ls test.txt");

    chdir("..");
    system("rm -r temp_test");

    return 0;
}
