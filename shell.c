#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARG_COUNT 10
#define MAX_HISTORY_SIZE 20
#define UP_ARROW 65
#define DOWN_ARROW 66
#define ESCAPE_KEY 27
#define BACKSPACE 127

// Global variables to hold process IDs
pid_t pid = -1;
pid_t pipe_pid = -1;

int flag = 1;
typedef struct
{
    char name[MAX_COMMAND_LENGTH];
    char value[MAX_COMMAND_LENGTH];
} Variable;

Variable variables[MAX_ARG_COUNT];
int variable_count = 0;

// Global variable to store the exit status of the last executed command
int last_exit_status = 0;

char command_history[MAX_HISTORY_SIZE][MAX_COMMAND_LENGTH];
int history_count = 0;
int current_history_index = -1;

struct termios orig_termios;

void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void print_status()
{
    printf("Last command exit status: %d\n", last_exit_status);
}

void handle_sigint()
{
    printf("\nYou typed Control-C!\n");

    // Check if the process IDs are valid and active
    if (pid > 0)
    {
        // Kill the child process or process group
        killpg(pid, SIGKILL);
        printf("Stopping process with PID %d\n", pid);
    }

    if (pipe_pid > 0)
    {
        // Kill the child process or process group
        killpg(pipe_pid, SIGKILL);
        printf("Stopping process with PID %d\n", pipe_pid);
    }
}

void parse_command(char *command, char **argv1, char **argv2, int *piping)
{
    char *token;
    int i = 0;
    *piping = 0;

    // First part of the command
    token = strtok(command, " ");
    while (token != NULL)
    {
        argv1[i] = token;
        token = strtok(NULL, " ");
        i++;
        if (token && strcmp(token, "|") == 0)
        {
            *piping = 1;
            break;
        }
    }
    argv1[i] = NULL;

    // Second part of the command if piping
    if (*piping)
    {
        i = 0;
        token = strtok(NULL, " ");
        while (token != NULL)
        {
            argv2[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv2[i] = NULL;
    }
}

char *get_variable_value(const char *name)
{
    for (int i = 0; i < variable_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            return variables[i].value;
        }
    }
    return NULL;
}

void set_variable_value(const char *name, const char *value)
{
    if (variable_count < MAX_ARG_COUNT)
    {
        strcpy(variables[variable_count].name, name);
        strcpy(variables[variable_count].value, value);
        variable_count++;
    }
    else
    {
        fprintf(stderr, "Max variable count reached.\n");
    }
}

void add_to_history(const char *command)
{
    if (history_count < MAX_HISTORY_SIZE)
    {
        strcpy(command_history[history_count], command);
        history_count++;
    }
    else
    {
        // Shift existing history to make space for new command
        for (int i = 0; i < MAX_HISTORY_SIZE - 1; i++)
        {
            strcpy(command_history[i], command_history[i + 1]);
        }
        strcpy(command_history[MAX_HISTORY_SIZE - 1], command);
    }
    current_history_index = history_count;
}

void display_command_from_history(char *command, const char *prompt_name)
{
    if (current_history_index >= 0 && current_history_index < history_count)
    {
        strcpy(command, command_history[current_history_index]);
        printf("\r%s: %s\033[K", prompt_name, command); // Clear line after the command
        fflush(stdout);
    }
}

void handle_arrow_key_press(int key, char *command, const char *prompt_name)
{
    if (key == UP_ARROW)
    {
        if (current_history_index > 0)
        {
            current_history_index--;
            display_command_from_history(command, prompt_name);
        }
    }
    else if (key == DOWN_ARROW)
    {
        if (current_history_index < history_count)
        {
            current_history_index++;
            if (current_history_index == history_count)
            {
                // Clear the line for new command
                printf("\r%s: \033[K", prompt_name);
                fflush(stdout);
                command[0] = '\0'; // Clear the command buffer
            }
            else
            {
                display_command_from_history(command, prompt_name);
            }
        }
    }
}

void read_input_with_history(char *command, const char *prompt_name)
{
    enable_raw_mode();
    int c;
    int pos = 0;
    memset(command, 0, MAX_COMMAND_LENGTH);

    printf("%s: ", prompt_name);
    fflush(stdout);

    while (1)
    {
        signal(SIGINT, handle_sigint);

        if ((c = getchar()) == EOF)
        {
            printf("%s: ", prompt_name);
            continue;
        }

        if (c == ESCAPE_KEY)
        {
            if (getchar() == '[')
            {
                switch (getchar())
                {
                case 'A': // Up arrow
                    handle_arrow_key_press(UP_ARROW, command, prompt_name);
                    pos = strlen(command);
                    break;
                case 'B': // Down arrow
                    handle_arrow_key_press(DOWN_ARROW, command, prompt_name);
                    pos = strlen(command);
                    break;
                }
            }
        }
        else if (c == '\n')
        {
            command[pos] = '\0';
            printf("\n");
            break;
        }
        else if (c == BACKSPACE)
        {
            if (pos > 0)
            {
                pos--;
                command[pos] = '\0';
                printf("\b \b"); // Move cursor back, print space, move cursor back again
                fflush(stdout);
            }
        }
        else
        {
            if (pos < MAX_COMMAND_LENGTH - 1)
            {
                command[pos++] = c;
                printf("%c", c);
                fflush(stdout);
            }
        }
    }
    disable_raw_mode();
}

int main()
{
    char command[MAX_COMMAND_LENGTH];
    char curr_command[MAX_COMMAND_LENGTH];
    char *argv1[MAX_ARG_COUNT], *argv2[MAX_ARG_COUNT];
    char last_command[MAX_COMMAND_LENGTH] = "";
    int piping, retid, status;
    int fildes[2];
    char *outfile, *errfile;
    int fd, fd_err, amper, redirect_out, redirect_err, redirect_out_app;
    char *prompt_name = malloc(strlen("hello") + 1);
    if (prompt_name == NULL)
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    strcpy(prompt_name, "hello");

    // Save the original stderr file descriptor
    int original_stderr = dup(STDERR_FILENO);

    // Register the signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    while (1)
    {
        // Register the signal handler for SIGINT
        signal(SIGINT, handle_sigint);

        command[strlen(command) - 1] = '\0'; // Remove trailing newline

        read_input_with_history(command, prompt_name);

        // Check for the !! command
        if (strcmp(command, "!!") == 0)
        {
            if (strlen(last_command) == 0)
            {
                printf("No previous command to repeat.\n");
                continue;
            }
            strcpy(command, last_command);
        }
        else
        {
            strcpy(last_command, command); // Store the current command as the last command
        }
        strcpy(curr_command, command);
        parse_command(command, argv1, argv2, &piping);

        // Check if the command is empty
        if (argv1[0] == NULL)
            continue;

        // Check for background execution
        int argc1 = 0;
        while (argv1[argc1] != NULL)
            argc1++;

        if (argc1 > 0 && strcmp(argv1[argc1 - 1], "&") == 0)
        {
            amper = 1;
            argv1[argc1 - 1] = NULL;
        }
        else
        {
            amper = 0;
        }

        // Check for variable substitution
        for (int i = 0; argv1[i] != NULL; i++)
        {
            char *value = get_variable_value(argv1[i]);
            if (value != NULL)
            {
                argv1[i] = value;
            }
        }

        add_to_history(curr_command);

        // Check for output redirection
        if (argc1 > 2 && strcmp(argv1[argc1 - 2], ">") == 0)
        {
            redirect_out = 1;
            argv1[argc1 - 2] = NULL;
            outfile = argv1[argc1 - 1];
        }
        else if (argc1 > 2 && strcmp(argv1[argc1 - 2], "2>") == 0)
        {
            redirect_err = 1;
            argv1[argc1 - 2] = NULL;
            errfile = argv1[argc1 - 1];
        }
        else if (argc1 > 2 && strcmp(argv1[argc1 - 2], ">>") == 0)
        {
            redirect_out_app = 1;
            argv1[argc1 - 2] = NULL;
            outfile = argv1[argc1 - 1];
        }
        else
        {
            redirect_out = 0;
            redirect_out_app = 0;
            redirect_err = 0;
        }

        // Check for built-in commands
        if (argc1 > 1 && strcmp(argv1[0], "prompt") == 0)
        {
            free(prompt_name); // Free the previous prompt name memory
            prompt_name = malloc(strlen(argv1[argc1 - 1]) + 1);
            if (prompt_name == NULL)
            {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }
            strcpy(prompt_name, argv1[argc1 - 1]);
            continue;
        }

        else if (argc1 > 1 && strcmp(argv1[0], "echo") == 0)
        {
            if (strcmp(argv1[1], "$?") == 0)
            {
                printf("%d \n", last_exit_status);
            }
            else
            {
                for (int i = 1; i < argc1; i++)
                {
                    printf("%s ", argv1[i]);
                }
                printf("\n");
            }
            continue;
        }
        else if (argc1 > 1 && strcmp(argv1[0], "cd") == 0)
        {
            if (chdir(argv1[1]) != 0)
            {
                perror("chdir failed");
            }
            continue;
        }
        else if (argc1 == 1 && strcmp(argv1[0], "quit") == 0)
        {
            exit(EXIT_SUCCESS);
        }
        else if (argc1 > 2 && argv1[argc1 - 2] != NULL && strcmp(argv1[argc1 - 2], "=") == 0)
        {
            set_variable_value(argv1[argc1 - 3], argv1[argc1 - 1]);
            continue;
        }

        // Fork and execute the command
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork failed");
            continue;
        }

        if (pid == 0)
        { // Child process

            // Set the process group ID for the child process
            setpgid(0, 0);
            if (redirect_out)
            {
                fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                if (fd < 0)
                {
                    perror("open failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (redirect_out_app)
            {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
                if (fd < 0)
                {
                    perror("open failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (redirect_err)
            {
                printf("hello");
                fd_err = open(errfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                if (fd_err < 0)
                {
                    perror("open errfile failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }

            if (piping)
            {
                if (pipe(fildes) < 0)
                {
                    perror("pipe failed");
                    exit(EXIT_FAILURE);
                }
                pid_t pipe_pid = fork();
                if (pipe_pid < 0)
                {
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                }
                if (pipe_pid == 0)
                { // First component of the pipe
                    close(STDOUT_FILENO);
                    dup2(fildes[1], STDOUT_FILENO);
                    close(fildes[0]);
                    close(fildes[1]);
                    execvp(argv1[0], argv1);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                }
                else
                { // Second component of the pipe
                    close(STDIN_FILENO);
                    dup2(fildes[0], STDIN_FILENO);
                    close(fildes[0]);
                    close(fildes[1]);
                    execvp(argv2[0], argv2);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                execvp(argv1[0], argv1);
                perror("execvp failed");
                exit(errno);
            }
        }

        // Parent process waits for the child if not running in background
        if (!amper)
        {
            // Set the process group ID for the parent process
            setpgid(pid, pid);

            retid = waitpid(pid, &status, WUNTRACED);
            if (retid < 0)
            {
                perror("waitpid failed");
            }
            else if (WIFEXITED(status))
            {
                last_exit_status = WEXITSTATUS(status);
            }
            else
            {
                last_exit_status = -1; // Indicate an abnormal termination
            }
            // Reset the process IDs
            pid = -1;
            pipe_pid = -1;
        }

        // Restore stderr to the original file descriptor if it was redirected
        if (redirect_err)
        {
            dup2(original_stderr, STDERR_FILENO);
            redirect_err = 0;
        }
    }

    // Close the original stderr file descriptor
    close(original_stderr);
    free(prompt_name);

    return 0;
}
