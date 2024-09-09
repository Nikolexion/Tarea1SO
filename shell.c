#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_NUM_ARGS 64

// Definimos un struct para manejar los comandos favoritos en memoria dinamica
typedef struct {
    int id;
    char command[MAX_COMMAND_LENGTH];
} Favorito;

Favorito favoritos[MAX_NUM_ARGS];
int num_favorites = 0;
char favs_file[MAX_COMMAND_LENGTH] = "";
char ultimo_comando[MAX_COMMAND_LENGTH] = "";

// Funcion para recortar espacios iniciales y finales
char *cut_spaces(char *str) {
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Funcion para analizar el comando y sus argumentos
void parse_command(char *command, char **args) {
    command = cut_spaces(command);  
    for (int i = 0; i < MAX_NUM_ARGS; i++) {
        args[i] = strsep(&command, " ");
        if (args[i] == NULL) break;
        args[i] = cut_spaces(args[i]);  
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
        // Error haciendo fork
        perror("Error forking");
    } else {
        int status;
        waitpid(pid, &status, 0);
        
        // Solo agregar el comando si fue exitoso (status == 0)
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            if (num_favorites < MAX_NUM_ARGS) {
                favs_agregar(args[0]);
            } else {
                printf("Error: Número máximo de favoritos alcanzado.\n");
            }
        }
    }
}

// Funcion para ejecutar comandos con pipes
void execute_piped_commands(char **commands, int num_commands) {
    int pipefd[2];
    pid_t pid;
    int fd_in = 0;
    int guardar = 0;

    
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
                perror("Error ejecutando el comando");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            // Error haciendo fork
            perror("Error haciendo fork");
        } else {
            int status;
            waitpid(pid, &status, 0);
           
            // Solo agregar el comando si fue exitoso (status == 0)
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                guardar = 1;
            }
            close(pipefd[1]);
            fd_in = pipefd[0]; 
        }
    }
    if(guardar == 0){
        favs_agregar(ultimo_comando);
    }
}

// Funcion para dividir el comando por pipes
int split_by_pipe(char *command, char **commands) {
    int i = 0;
    while ((commands[i] = strsep(&command, "|")) != NULL) {
        commands[i] = cut_spaces(commands[i]);  
        i++;
    }
    return i;
}

// Manejador de la señal SIGCHLD
void sigchld_handler(int signum) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
    //printf("\nshell:$ "); // Reimprimir el prompt cuando termine un proceso hijo
    fflush(stdout);
}

// Funcion para establecer un recordatorio
void set_reminder(int seconds, const char *message) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Proceso hijo para el recordatorio
        sleep(seconds);
        printf("\n[Recordatorio]: %s\n", message);
        printf("shell:$ ");
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        perror("Error haciendo fork para el recordatorio");
    }
}

// Funcion para guardar la ruta de favs en un archivo config.txt
void save_favs_file_path(const char *path) {
    FILE *config_file = fopen("config.txt", "w");
    if (config_file) {
        fprintf(config_file, "%s\n", path);
        fclose(config_file);
    } else {
        perror("Error al guardar la ruta del archivo de favoritos");
    }
}

// Cargar la ruta de favs
void load_favs_file_path() {
    FILE *config_file = fopen("config.txt", "r");
    if (config_file) {
        if (fgets(favs_file, MAX_COMMAND_LENGTH, config_file)) {
            // Elimina el salto de línea
            favs_file[strcspn(favs_file, "\n")] = '\0';
        }
        fclose(config_file);
    }
}

void favs_set(char *path);

// Funcion para crear un archivo de favoritos
void favs_crear(char *path) {
    favs_set(path);
    strncpy(favs_file, path, MAX_COMMAND_LENGTH);
    FILE *file = fopen(favs_file, "w");
    if (file) {
        fclose(file);
        printf("Archivo de favoritos creado en: %s\n", favs_file);
    } else {
        perror("Error al crear el archivo de favoritos");
    }
}

// Funcion para mostrar los comandos favoritos en memoria
void favs_mostrar() {
    for (int i = 0; i < num_favorites; i++) {
        printf("%d: %s\n", favoritos[i].id, favoritos[i].command);
    }
}

// Funcion para eliminar comandos favoritos en memoria
void favs_eliminar(char *ids) {
    char *id_str = strtok(ids, ",");
    while (id_str) {
        int id = atoi(id_str);
        for (int i = 0; i < num_favorites; i++) {
            if (favoritos[i].id == id) {
                for (int j = i; j < num_favorites - 1; j++) {
                    favoritos[j] = favoritos[j + 1];
                    favoritos[j].id = j + 1;
                }
                num_favorites--;
                break;
            }
        }
        id_str = strtok(NULL, ",");
    }
}

// Funcion para buscar un comando favorito en especifico
void favs_buscar(char *cmd) {
    for (int i = 0; i < num_favorites; i++) {
        if (strstr(favoritos[i].command, cmd)) {
            printf("%d: %s\n", favoritos[i].id, favoritos[i].command);
        }
    }
}

// Funcion para borrar todos los comandos favoritos
void favs_borrar() {
    num_favorites = 0;
    printf("Todos los comandos favoritos han sido borrados.\n");
}

// Funcion para ejecutar un comando favorito
void favs_ejecutar(int id) { 
    for (int i = 0; i < num_favorites; i++) {
        if (favoritos[i].id == id) {
            char *commands[MAX_NUM_ARGS];
            strcpy(ultimo_comando, favoritos[i].command);
            char *command;
            strcpy(command, favoritos[i].command);
            int num_commands = split_by_pipe(command, commands);
            if (num_commands > 1) {
                execute_piped_commands(commands, num_commands);
            } else {
                char *args[MAX_NUM_ARGS];
                parse_command(favoritos[i].command, args);
                execute_single_command(args);
            }
            break;
        }
    }
}

// Funcion para cargar los favoritos desde el archivo
void favs_cargar() {
    FILE *file = fopen(favs_file, "r");
    if (file) {
        char line[MAX_COMMAND_LENGTH];
        num_favorites = 0;  // Reinicia el contador de favoritos
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0; // Eliminar salto de línea
            favoritos[num_favorites].id = num_favorites + 1; // Actualizar el índice correctamente
            strncpy(favoritos[num_favorites].command, line, MAX_COMMAND_LENGTH);
            num_favorites++;
        }
        fclose(file);
        printf("Favoritos cargados desde: %s\n", favs_file);
        for (int i = 0; i < num_favorites; i++) {
            printf("%d: %s\n", favoritos[i].id, favoritos[i].command);
        }
    } else {
        perror("Error al cargar el archivo de favoritos");
    }
}

// Funcion para guardar los favoritos en el archivo
void favs_guardar() {
    if (strlen(favs_file) == 0) {
        printf("Error: No se ha establecido un archivo de favoritos.\n");
        return;
    }

    // Abre el archivo en modo lectura para verificar si el comando ya está
    FILE *file = fopen(favs_file, "r");
    if (!file) {
        perror("Error al abrir el archivo de favoritos para lectura");
        return;
    }

    // Variable para leer las líneas del archivo
    char line[MAX_COMMAND_LENGTH];
    char existing_commands[num_favorites][MAX_COMMAND_LENGTH];
    int existing_count = 0;

    // Leer todos los comandos existentes en el archivo
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; // Elimina el salto de línea
        strcpy(existing_commands[existing_count++], line);
    }
    fclose(file);

    // Abre el archivo en modo append 
    file = fopen(favs_file, "a");
    if (!file) {
        perror("Error al abrir el archivo de favoritos para escritura");
        return;
    }

    // Guardar los comandos únicos
    for (int i = 0; i < num_favorites; i++) {
        int found = 0;
        for (int j = 0; j < existing_count; j++) {
            if (strcmp(favoritos[i].command, existing_commands[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(file, "%s\n", favoritos[i].command);
        }
    }

    fclose(file);
    printf("Favoritos guardados en: %s\n", favs_file);
}

// Agrega el comando a la lista en memoria, evitando repeticiones y comandos asociados a favs
void favs_agregar(char *cmd) {
    
    for (int i = 0; i < num_favorites; i++) {
        if (strcmp(favoritos[i].command, cmd) == 0) {
            return;
        }
    }

    if (strncmp(cmd, "favs", 4) == 0){
        return;
    }
    if (num_favorites < MAX_NUM_ARGS) {
        favoritos[num_favorites].id = num_favorites + 1;
        strncpy(favoritos[num_favorites].command, cmd, MAX_COMMAND_LENGTH);
        num_favorites++;
        //favs_guardar(); // Guardar automáticamente al agregar
        printf("Comando añadido a favoritos: %s\n", cmd);
    } else {
        printf("Error: Número máximo de favoritos alcanzado.\n");
    }
}

// Funcion para setear la ruta de favs
void favs_set(char *path) {
    strncpy(favs_file, path, MAX_COMMAND_LENGTH);
    favs_file[MAX_COMMAND_LENGTH - 1] = '\0'; 
    save_favs_file_path(favs_file);
}

// Administrador de comandos favs
void handle_favs_command(char *command) {
    // Separar la subcomando y sus argumentos
    char *args[MAX_NUM_ARGS];
    parse_command(command, args);

    if (strcmp(args[0], "crear") == 0) {
        // Lógica para crear el archivo de favoritos
        favs_crear(args[1]);
    } else if (strcmp(args[0], "mostrar") == 0) {
        // Lógica para mostrar los favoritos en memoria
        favs_mostrar();
    } else if (strcmp(args[0], "eliminar") == 0) {
        // Lógica para eliminar favoritos segun su indice en memoria
        favs_eliminar(args[1]);
    } else if (strcmp(args[0], "buscar") == 0) {
        // Lógica para buscar favoritos
        favs_buscar(args[1]);
    } else if (strcmp(args[0], "borrar") == 0) {
        // Lógica para borrar todos los favoritos en memoria
        favs_borrar();
    } else if (strcmp(args[0], "guardar") == 0) {
        // Lógica para guardar los favoritos en el archivo
        favs_guardar();
    } else if (strcmp(args[0], "cargar") == 0) {
        // Lógica para cargar los favoritos del archivo
        favs_cargar();
    } else if (strcmp(args[0], "ejecutar") == 0) {
        // Lógica para ejecutar un comando de la memoria
        favs_ejecutar(atoi(args[1]));
    } else {
        fprintf(stderr, "Error: Subcomando favs no reconocido: %s\n", args[0]);
    }
}

// Funcion main
int main() {
    struct sigaction sa;
    sa.sa_handler = &sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("Error setting up SIGCHLD handler");
        exit(EXIT_FAILURE);
    }
    char command[MAX_COMMAND_LENGTH];
    char *commands[MAX_NUM_ARGS];
    load_favs_file_path();

    // Intenta cargar el archivo de favoritos al inicio
    if (strlen(favs_file) > 0) {
        favs_set(favs_file);
        //favs_cargar();
    }

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
            // Analiza el comando para obtener el tiempo y el mensaje
            char *args[MAX_NUM_ARGS];
            parse_command(command, args);

            if (args[2] != NULL && args[3] != NULL) {
                int seconds = atoi(args[2]);
                char message[MAX_COMMAND_LENGTH] = "";

                // Une los argumentos del mensaje en una sola cadena
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

        // Maneja el comando favs
        if (strncmp(command, "favs", 4) == 0) {
            handle_favs_command(command + 5);  // Pasa la parte del comando después de "favs "
            continue;
        }
        
        strcpy(ultimo_comando, command);
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