#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_NUM_ARGS 64

// Function to trim leading and trailing spaces
char *trim_spaces(char *str) {
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Function to parse the command and its arguments
void parse_command(char *command, char **args) {
    command = trim_spaces(command);  // Trim any leading/trailing spaces
    for (int i = 0; i < MAX_NUM_ARGS; i++) {
        args[i] = strsep(&command, " ");
        if (args[i] == NULL) break;
        args[i] = trim_spaces(args[i]);  // Trim spaces around each argument
        if (strlen(args[i]) == 0) i--;  // Remove empty arguments caused by extra spaces
    }
}

// Function to execute a single command
void execute_single_command(char **args) {
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

// Function to execute commands with pipes
void execute_piped_commands(char **commands, int num_commands) {
    int pipefd[2];
    pid_t pid;
    int fd_in = 0;

    for (int i = 0; i < num_commands; i++) {
        pipe(pipefd);
        pid = fork();
        
        if (pid == 0) {
            // Child process
            dup2(fd_in, 0); // Change the input according to the old one
            if (i < num_commands - 1) {
                dup2(pipefd[1], 1); // Change the output to the pipe
            }
            close(pipefd[0]);
            
            char *args[MAX_NUM_ARGS];
            parse_command(commands[i], args);
            if (execvp(args[0], args) == -1) {
                perror("Error executing command");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            // Error forking
            perror("Error forking");
        } else {
            // Parent process
            wait(NULL);
            close(pipefd[1]);
            fd_in = pipefd[0]; // Save the input for the next command
        }
    }
}

// Function to split the command by pipes
int split_by_pipe(char *command, char **commands) {
    int i = 0;
    while ((commands[i] = strsep(&command, "|")) != NULL) {
        commands[i] = trim_spaces(commands[i]);  // Trim spaces around each command
        i++;
    }
    return i;
}

// Main function
int main() {
    char command[MAX_COMMAND_LENGTH];
    char *commands[MAX_NUM_ARGS];
    
    while (1) {
        printf("shell:$ ");
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

        int num_commands = split_by_pipe(command, commands);
        if (num_commands > 1) {
            execute_piped_commands(commands, num_commands);
        } else {
            char *args[MAX_NUM_ARGS];
            parse_command(command, args);
            execute_single_command(args);
        }
    }
    
    return 0;
}
