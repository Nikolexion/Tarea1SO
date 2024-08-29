#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_NUM_ARGS 64

// Funcion para recortar espacios iniciales y finales
char *trim_spaces(char *str) {
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Funcion para analizar el comando y sus argumentos
void parse_command(char *command, char **args) {
    command = trim_spaces(command);  
    for (int i = 0; i < MAX_NUM_ARGS; i++) {
        args[i] = strsep(&command, " ");
        if (args[i] == NULL) break;
        args[i] = trim_spaces(args[i]);  
        if (strlen(args[i]) == 0) i--;  
    }
}

// Funcion para ejecutar un solo comando
void execute_single_command(char **args) {
    pid_t pid = fork();
    
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            if (errno == ENOENT) {
                fprintf(stderr, "Error: orden no encontrada: %s\n", args[0]);
            } else {
                perror("Error ejecutando el comando");
            }
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("Error forking");
    } else {
        wait(NULL);
    }
}

// Funcion para ejecutar comandos con pipes
void execute_piped_commands(char **commands, int num_commands) {
    int pipefd[2];
    pid_t pid;
    int fd_in = 0;

    for (int i = 0; i < num_commands; i++) {
        pipe(pipefd);
        pid = fork();
        
        if (pid == 0) {
            dup2(fd_in, 0); 
            if (i < num_commands - 1) {
                dup2(pipefd[1], 1); 
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
            wait(NULL);
            close(pipefd[1]);
            fd_in = pipefd[0]; 
        }
    }
}

// Funcion para dividir el comando por pipes
int split_by_pipe(char *command, char **commands) {
    int i = 0;
    while ((commands[i] = strsep(&command, "|")) != NULL) {
        commands[i] = trim_spaces(commands[i]);  
        i++;
    }
    return i;
}

// Funcion para establecer un recordatorio
void set_reminder(int seconds, const char *message) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process for reminder
        sleep(seconds);
        printf("\n[Recordatorio]: %s\n", message);
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        perror("Error forking reminder process");
    }
}

// Funcion main
int main() {
    char command[MAX_COMMAND_LENGTH];
    char *commands[MAX_NUM_ARGS];
    
    while (1) {
        printf("shell:$ ");
        fgets(command, MAX_COMMAND_LENGTH, stdin);

        // Quitamos el salto de linea
        command[strcspn(command, "\n")] = 0;

        // Maneja "enter" sin comando
        if (strlen(command) == 0) {
            continue;
        }

        // Maneja el comando de salida
        if (strcmp(command, "exit") == 0) {
            break;
        }

        // Maneja el comando de recordatorio
        if (strncmp(command, "set recordatorio", 16) == 0) {
            // Parse the command to get time and message
            char *args[MAX_NUM_ARGS];
            parse_command(command, args);

            if (args[2] != NULL && args[3] != NULL) {
                int seconds = atoi(args[2]);
                char message[MAX_COMMAND_LENGTH] = "";

                // Join the remaining arguments as the message
                for (int i = 3; args[i] != NULL; i++) {
                    strcat(message, args[i]);
                    if (args[i + 1] != NULL) strcat(message, " ");  
                }

                set_reminder(seconds, message);
                continue;
            } else {
                printf("Usage: set recordatorio <seconds> \"<message>\"\n");
                continue;
            }
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
