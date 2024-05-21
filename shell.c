#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARG_COUNT 10


typedef struct {
    char name[MAX_COMMAND_LENGTH];
    char value[MAX_COMMAND_LENGTH];
} Variable;

Variable variables[MAX_ARG_COUNT];
int variable_count = 0;

void parse_command(char *command, char **argv1, char **argv2, int *piping) {
    char *token;
    int i = 0;
    *piping = 0;

    // First part of the command
    token = strtok(command, " ");
    while (token != NULL) {
        argv1[i] = token;
        token = strtok(NULL, " ");
        i++;
        if (token && strcmp(token, "|") == 0) {
            *piping = 1;
            break;
        }
    }
    argv1[i] = NULL;

    // Second part of the command if piping
    if (*piping) {
        i = 0;
        token = strtok(NULL, " ");
        while (token != NULL) {
            argv2[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv2[i] = NULL;
    }
}

char* get_variable_value(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void set_variable_value(const char *name, const char *value) {
    if (variable_count < MAX_ARG_COUNT) {
        strcpy(variables[variable_count].name, name);
        strcpy(variables[variable_count].value, value);
        variable_count++;
    } else {
        fprintf(stderr, "Max variable count reached.\n");
    }
}



int main() {
    char command[MAX_COMMAND_LENGTH];
    char *argv1[MAX_ARG_COUNT], *argv2[MAX_ARG_COUNT];
    char last_command[MAX_COMMAND_LENGTH] = "";
    int piping, retid, status;
    int fildes[2];
    char *outfile, *errfile;
    int fd, fd_err, amper, redirect_out, redirect_err, redirect_out_app;
    char *prompt_name = malloc(strlen("hello") + 1);
    if (prompt_name == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    strcpy(prompt_name, "hello");

    // Save the original stderr file descriptor
    int original_stderr = dup(STDERR_FILENO);

    while (1) {
        printf("%s: ", prompt_name);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            perror("fgets failed");
            continue;
        }
        command[strlen(command) - 1] = '\0'; // Remove trailing newline

        // Check for the !! command
        if (strcmp(command, "!!") == 0) {
            if (strlen(last_command) == 0) {
                printf("No previous command to repeat.\n");
                continue;
            }
            strcpy(command, last_command);
        } else {
            strcpy(last_command, command); // Store the current command as the last command
        }

        parse_command(command, argv1, argv2, &piping);

        // Check if the command is empty
        if (argv1[0] == NULL)
            continue;

        // Check for background execution
        int argc1 = 0;
        while (argv1[argc1] != NULL) argc1++;

        if (argc1 > 0 && strcmp(argv1[argc1 - 1], "&") == 0) {
            amper = 1;
            argv1[argc1 - 1] = NULL;
        } else {
            amper = 0;
        }
        


        // Check for variable substitution
        for (int i = 0; argv1[i] != NULL; i++) {
            char *value = get_variable_value(argv1[i]);
            if (value != NULL) {
                argv1[i] = value;
            }
        }

        // Check for output redirection
        if (argc1 > 2 && strcmp(argv1[argc1 - 2], ">") == 0) {
            redirect_out = 1;
            argv1[argc1 - 2] = NULL;
            outfile = argv1[argc1 - 1];
        } else if (argc1 > 2 && strcmp(argv1[argc1 - 2], "2>") == 0){
            redirect_err = 1;
            argv1[argc1 - 2] = NULL;
            errfile = argv1[argc1 - 1];
        }
        else if (argc1 > 2 && strcmp(argv1[argc1 - 2], ">>") == 0){
            redirect_out_app = 1;
            argv1[argc1 - 2] = NULL;
            outfile = argv1[argc1 - 1];
        }
        else if (argc1 > 2 && strcmp(argv1[argc1 - 3], "prompt") == 0 && strcmp(argv1[argc1 - 2], "=") == 0){
            // Free the previously allocated memory, if any
            free(prompt_name);

            // Allocate memory for the new prompt name
            prompt_name = malloc(strlen(argv1[argc1 - 1]) + 1);
            if (prompt_name == NULL) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }

            // Copy the new prompt name
            strcpy(prompt_name, argv1[argc1 - 1]);

            continue;
        }
        else if (argc1 > 1 && strcmp(argv1[0], "echo") == 0) {
            for (int i = 1; i < argc1; i++) {
                printf("%s ", argv1[i]);
            }
            printf("\n");
            continue;
        }
        else if (argc1 > 1 && strcmp(argv1[0], "cd") == 0) {
            if (chdir(argv1[1]) != 0) {
                perror("chdir failed");
            }
            continue;
        }
        else if (argc1 == 1 && strcmp(argv1[0], "quit") == 0) {
            exit(EXIT_SUCCESS);
        }
        else if (argc1 > 2 && strcmp(argv1[argc1 - 2], "=") == 0) {
            set_variable_value(argv1[argc1 - 3], argv1[argc1 - 1]);
            continue;
        }

        else{
            redirect_out = 0;
            redirect_out_app = 0;
            redirect_err = 0;
        } 

        // Fork and execute the command
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) { // Child process
            if (redirect_out) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                if (fd < 0) {
                    perror("open failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (redirect_out_app) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
                if (fd < 0) {
                    perror("open failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (redirect_err) {
                fd_err = open(errfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                if (fd_err < 0) {
                    perror("open errfile failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }


            if (piping) {
                if (pipe(fildes) < 0) {
                    perror("pipe failed");
                    exit(EXIT_FAILURE);
                }
                pid_t pipe_pid = fork();
                if (pipe_pid < 0) {
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                }
                if (pipe_pid == 0) { // First component of the pipe
                    close(STDOUT_FILENO);
                    dup2(fildes[1], STDOUT_FILENO);
                    close(fildes[0]);
                    close(fildes[1]);
                    execvp(argv1[0], argv1);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                } else { // Second component of the pipe
                    close(STDIN_FILENO);
                    dup2(fildes[0], STDIN_FILENO);
                    close(fildes[0]);
                    close(fildes[1]);
                    execvp(argv2[0], argv2);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                }
            } else {
                execvp(argv1[0], argv1);
                // perror("execvp failed");
                // exit(EXIT_FAILURE);
            }
        }

        // Parent process waits for the child if not running in background
        if (!amper) {
            retid = waitpid(pid, &status, 0);
            if (retid < 0) {
                perror("waitpid failed");
            }
        }

        // Restore stderr to the original file descriptor if it was redirected
        if (redirect_err) {
            dup2(original_stderr, STDERR_FILENO);
            redirect_err = 0;
        }
    }

    // Close the original stderr file descriptor
    close(original_stderr);

    return 0;
}