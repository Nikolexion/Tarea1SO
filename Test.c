#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_NUM_ARGS 64

// Function to parse the command and its arguments
void parse_command(char *command, char **args) {
    for (int i = 0; i < MAX_NUM_ARGS; i++) {
        args[i] = strsep(&command, " ");
        if (args[i] == NULL) break;
    }
}

// Function to execute the command
void execute_command(char **args) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("Error forking");
    } else {
        // Parent process
        wait(NULL);
    }
}

// Main function
int main() {
    char command[MAX_COMMAND_LENGTH];
    char *args[MAX_NUM_ARGS];
    
    while (1) {
        printf("mishell:$ ");
        fgets(command, MAX_COMMAND_LENGTH, stdin);

        // Remove trailing newline character
        command[strcspn(command, "\n")] = 0;

        // Handle "enter" without a command
        if (strlen(command) == 0) {
            continue;
        }

        // Handle exit command
        if (strcmp(command, "exit") == 0) {
            break;
        }

        parse_command(command, args);
        execute_command(args);
    }
    
    return 0;
}
