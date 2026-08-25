#include <stdio.h> // Required for file operations (fopen, fprintf, fclose)
#include <stdlib.h> // Required for system() function

int main() {
    FILE *fptr; // Declare a file pointer

    // Open the file in write mode ("w").
    // If file.txt does not exist, it will be created.
    // If file.txt exists, its content will be truncated (overwritten).
    fptr = fopen("data/day.txt", "a");

    // Check if the file was opened successfully
    if (fptr == NULL) {
    //    printf("Error: Could not open file.txt\n");
        return 1; // Indicate an error
    }

    // Write "Hello, World!" to the file
    fprintf(fptr, "Hello, DAy!\n");

    // Close the file
    fclose(fptr);

    // Run the analysis loop to update stock prices
    system("./+x/analysis_loop.+x");

  //  printf("Successfully wrote 'Hello, World!' to file.txt\n");

    return 0; // Indicate successful execution
}
