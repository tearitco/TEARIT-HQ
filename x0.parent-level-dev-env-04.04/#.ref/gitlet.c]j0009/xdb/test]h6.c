#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

void check_pass_fail(int result, const char *test_name) {
    if (result == 0) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
    }
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int main() {
    system("mkdir temp_test");
    chdir("temp_test");

    printf("--- Testing init ---\n");
    int res = system("../gitlet init");
    check_pass_fail(res, "init");
    check_pass_fail(file_exists(".gitlet") ? 0 : -1, "init directory check");

    printf("\n--- Testing add ---\n");
    system("echo 'hello' > test.txt");
    res = system("../gitlet add test.txt");
    check_pass_fail(res, "add");
    check_pass_fail(file_exists(".gitlet/index") ? 0 : -1, "add index check");

    printf("\n--- Testing commit ---\n");
    res = system("../gitlet commit \"first commit\"");
    check_pass_fail(res, "commit");

    printf("\n--- Testing log ---\n");
    res = system("../gitlet log");
    check_pass_fail(res, "log");

    printf("\n--- Testing status ---\n");
    res = system("../gitlet status");
    check_pass_fail(res, "status");

    printf("\n--- Testing branch ---\n");
    res = system("../gitlet branch new-branch");
    check_pass_fail(res, "branch create");
    res = system("../gitlet branch");
    check_pass_fail(res, "branch list");

    printf("\n--- Testing checkout ---\n");
    res = system("../gitlet checkout new-branch");
    check_pass_fail(res, "checkout");

    printf("\n--- Testing rm ---\n");
    res = system("../gitlet rm test.txt");
    check_pass_fail(res, "rm");
    check_pass_fail(file_exists("test.txt") ? -1 : 0, "rm file check");

    printf("\n--- Testing push and pull ---\n");
    system("mkdir ../remote");
    res = system("cd ../remote && ../../gitlet init --bare");
    check_pass_fail(res, "remote init");

    // Push from local
    res = system("../gitlet checkout master");
    check_pass_fail(res, "checkout master");
    system("echo 'hello again' > test.txt");
    system("../gitlet add test.txt");
    system("../gitlet commit \"second commit\"");
    res = system("../gitlet push ../remote");
    check_pass_fail(res, "push");
    check_pass_fail(file_exists("../remote/.gitlet/objects") ? 0 : -1, "push objects check");

    res = system("../gitlet pull ../remote");
    check_pass_fail(res, "pull");

    printf("\n--- Testing merge ---\n");
    res = system("../gitlet checkout new-branch");
    check_pass_fail(res, "checkout new-branch");
    system("echo 'feature change' > feature.txt");
    system("../gitlet add feature.txt");
    system("../gitlet commit \"feature commit\"");
    res = system("../gitlet checkout master");
    check_pass_fail(res, "checkout master for merge");
    res = system("../gitlet merge new-branch");
    check_pass_fail(res, "merge");
    check_pass_fail(file_exists("feature.txt") ? 0 : -1, "merge file check");

    chdir("..");
    system("rm -rf temp_test");
    system("rm -rf remote");

    return 0;
}
