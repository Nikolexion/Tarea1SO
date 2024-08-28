#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_NUM_ARGS 64

// Funcion para darle formato al comando y a sus argumentos
void parse_command(char *command, char **args) {
    for (int i = 0; i < MAX_NUM_ARGS; i++) {
        args[i] = strsep(&command, " ");
        if (args[i] == NULL) break;
    }
}

// Funcion para ejecutar comando
void execute_command(char **args) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Proceso Hijo
        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error en el fork
        perror("Error forking");
    } else {
        // Proceso del padre
        wait(NULL);
    }
}


int main() {
    char command[MAX_COMMAND_LENGTH];
    char *args[MAX_NUM_ARGS];
    
    while (1) {
        printf("mishell:$ ");
        fgets(command, MAX_COMMAND_LENGTH, stdin);

        
        command[strcspn(command, "\n")] = 0;

        // Soportar input sin nada
        if (strlen(command) == 0) {
            continue;
        }

        // Soportar exit
        if (strcmp(command, "exit") == 0) {
            break;
        }

        parse_command(command, args);
        execute_command(args);
    }
    
    return 0;
}
