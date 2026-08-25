#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

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

int check_object_hash_format(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            if (strlen(ent->d_name) != 8) {
                closedir(dir);
                return -1;
            }
            for (int i = 0; i < 8; i++) {
                char c = ent->d_name[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                    closedir(dir);
                    return -1;
                }
            }
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return -1;
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
    check_pass_fail(check_object_hash_format(".gitlet/objects"), "add hash format check");

    printf("\n--- Testing commit ---\n");
    res = system("../gitlet commit \"first commit\"");
    check_pass_fail(res, "commit");
    check_pass_fail(check_object_hash_format(".gitlet/objects"), "commit hash format check");

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

    res = system("../gitlet checkout master");
    check_pass_fail(res, "checkout master");
    system("echo 'hello again' > test.txt");
    system("../gitlet add test.txt");
    system("../gitlet commit \"second commit\"");
    res = system("../gitlet push ../remote");
    check_pass_fail(res, "push");
    check_pass_fail(file_exists("../remote/.gitlet/objects") ? 0 : -1, "push objects check");
    check_pass_fail(check_object_hash_format("../remote/.gitlet/objects"), "push hash format check");

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

    printf("\n--- Testing fork ---\n");
    res = system("../gitlet fork ../my-fork");
    check_pass_fail(res, "fork");
    check_pass_fail(file_exists("../my-fork/.gitlet") ? 0 : -1, "fork directory check");
    check_pass_fail(file_exists("../my-fork/.gitlet/objects") ? 0 : -1, "fork objects check");
    check_pass_fail(file_exists("../my-fork/.gitlet/refs/heads/master") ? 0 : -1, "fork master ref check");
    check_pass_fail(check_object_hash_format("../my-fork/.gitlet/objects"), "fork hash format check");

    chdir("..");
    system("rm -rf temp_test");
    system("rm -rf remote");
    system("rm -rf my-fork");

    return 0;
}
