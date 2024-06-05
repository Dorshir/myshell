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
#include <ctype.h>

#define MAX_ARG_COUNT 10          // max pipes
#define MAX_COMMAND_LENGTH 1024   // command length
#define MAX_SUBCOMMAND_LENGTH 480 // subcommand length
#define MAX_SUBCOMMAND_COUNTER 10 // subcommand counter

#define MAX_HISTORY_SIZE 20
#define UP_ARROW 65
#define DOWN_ARROW 66
#define ESCAPE_KEY 27
#define BACKSPACE 127

void expand_commands(char ****argv, int *need_fork, int *argc, char *command);

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

char command[MAX_COMMAND_LENGTH];
char curr_command[MAX_COMMAND_LENGTH];
char last_command[MAX_COMMAND_LENGTH] = "";
int amper, redirect_out, redirect_err, redirect_out_app;
char *outfile, *errfile;
char *prompt_name;

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
    }

    if (pipe_pid > 0)
    {
        // Kill the child process or process group
        killpg(pipe_pid, SIGKILL);
    }
}

// Function to trim leading and trailing spaces
char *trim(char *str)
{
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0)
        return str; // All spaces?

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator
    *(end + 1) = 0;

    return str;
}

// Custom function to duplicate a string
char *my_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup)
    {
        memcpy(dup, s, len);
    }
    return dup;
}

// Function to split a string by a delimiter and handle multiple spaces
char **split_string(const char *str, const char delimiter, int *num_tokens)
{
    int count = 0;
    const char *temp = str;

    // Count the number of delimiters
    while (*temp)
    {
        if (*temp == delimiter)
            count++;
        temp++;
    }

    // Allocate memory for tokens
    char **tokens = malloc((count + 1) * sizeof(char *));
    if (tokens == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    int index = 0;
    char *start = my_strdup(str); // Duplicate the input string
    if (start == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        free(tokens);
        exit(EXIT_FAILURE);
    }

    char *end = strchr(start, delimiter);
    while (end != NULL)
    {
        *end = '\0';
        tokens[index++] = trim(start);
        start = end + 1;
        end = strchr(start, delimiter);
    }
    tokens[index++] = trim(start);

    *num_tokens = index;
    return tokens;
}

void argvAllocate(char ****argv)
{
    char ***argvVal = *argv;
    for (int i = 0; i < MAX_ARG_COUNT; i++)
    {
        argvVal[i] = (char **)(malloc(sizeof(char *) * 10));
        for (int j = 0; j < MAX_COMMAND_LENGTH; j++)
        {
            argvVal[i][j] = (char *)(malloc(sizeof(char *) * MAX_SUBCOMMAND_LENGTH));
        }
    }
}

void parse_command(char *command, char ****argv, int *argc, int *argv_count)
{
    int num_tokens;
    char ***argvArray = *argv;
    char **commands = split_string(command, '|', &num_tokens);

    *argv_count = num_tokens;
    for (int i = 0; i < num_tokens; i++)
    {
        printf("Command %d: %s\n", i + 1, commands[i]);

        // Tokenize each command by spaces
        int num_subtokens;

        argvArray[i] = split_string(commands[i], ' ', &num_subtokens);
        argc[i] = num_subtokens;
        for (int j = 0; j < num_subtokens; j++)
        {
            printf("  Subtoken [%d][%d]: %s\n", i, j, argvArray[i][j]);
        }
        // free(subtokens); // Free the memory allocated for subtokens
    }

    free(commands); // Free the memory allocated for commands
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
    // check if variable already exist
    for (int i = 0; i <= variable_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            strcpy(variables[i].value, value);
            return;
        }
    }

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

void handle_pipes(char ***argv, int *argc, int argv_count)
{
    //if ls | wc then echo bar else echo for fi
    printf("argv[0]: \n",argv[0][0]);
    printf("argv[1]: \n",argv[1][0]);
    int fildes[2];
    int fildes_prev[2];
    int status;
    pid_t pid;

    for (int i = 0; i < argv_count; i++)
    {
        if (i < argv_count - 1)
        {
            // Create a pipe
            if (pipe(fildes) == -1)
            {
                perror("pipe");
                exit(1);
            }
        }

        // Fork a child process
        pid = fork();
        if (pid == 0)
        {
            // Child process
            if (i > 0)
            {
                // Redirect input from the previous pipe
                dup2(fildes_prev[0], STDIN_FILENO);
                close(fildes_prev[0]);
                close(fildes_prev[1]);
            }
            if (i < argv_count - 1)
            {
                // Redirect output to the next pipe
                close(fildes[0]);
                dup2(fildes[1], STDOUT_FILENO);
                close(fildes[1]);
            }

            // Handle output redirection
            if (redirect_out)
            {
                int fd = creat(outfile, 0660);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if (redirect_err)
            {
                int fd_err = creat(errfile, 0660);
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }
            else if (redirect_out_app)
            {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (execvp(argv[i][0], argv[i]) == -1)
            {
                fprintf(stderr, "Command execution failed: %s\n", strerror(errno));
                exit(errno);
            }
        }
        else if (pid > 0)
        {
            // Parent process
            if (i > 0)
            {
                // Close the previous pipe
                close(fildes_prev[0]);
                close(fildes_prev[1]);
            }
            if (i < argv_count - 1)
            {
                // Save the current pipe for the next iteration
                fildes_prev[0] = fildes[0];
                fildes_prev[1] = fildes[1];
            }

            //ls
            //if true then !! else echo bar fi
            //work without fi

            // Wait for the child process to finish
            if (!amper)
            {
                waitpid(pid, &status, 0);
                last_exit_status = WEXITSTATUS(status);
            }
            else
            {
                printf("Process with PID %d is running in the background\n", pid);
            }
        }
        else
        {
            perror("fork");
            exit(1);
        }
    }
}

void execute_if_else(char* command)
{
    printf("command : %s\n",command);
    int argc;
    char ** argv1 = split_string(command, ' ' ,&argc);

    if (argc < 5 || strcmp(argv1[0], "if") != 0)
    {
        fprintf(stderr, "Invalid if statement syntax\n");
        return;
    }

    int then_index = -1;
    int else_index = -1;
    int fi_index = -1;

    // Find the positions of 'then', 'else', and 'fi' in the argument list
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv1[i], "then") == 0)
        {
            then_index = i;
        }
        else if (strcmp(argv1[i], "else") == 0)
        {
            else_index = i;
        }
        else if (strcmp(argv1[i], "fi") == 0)
        {
            fi_index = i;
        }
    }
    
    if(then_index == -1 || else_index == -1 || fi_index == -1){
        fprintf(stderr, "Invalid if statement syntax: must be then , else and fi\n");
        return;
    }

    // Ensure 'then' comes before 'else' if both are present
    if (then_index > else_index)
    {
        fprintf(stderr, "Invalid if statement syntax: 'then' must come before 'else'\n");
        return;
    }

    if (else_index > fi_index)
    {
        fprintf(stderr, "Invalid if statement syntax: 'else' must come before 'fi'\n");
        return;
    }
    

    // Extract the condition
    char *condition_argv[MAX_ARG_COUNT];
    for (int i = 1; i < then_index; i++)
    {
        condition_argv[i - 1] = argv1[i];
    }
    condition_argv[then_index - 1] = NULL;
    
    int argc1[MAX_SUBCOMMAND_COUNTER] = {0};
    int argv_count;
    char ***argv;
    

    parse_command(condition_argv, &argv, argc1, &argv_count);
    printf("argv[0]: \n",argv[0][0]);
    printf("argv[1]: \n",argv[1][0]);
    exit(1);
    // Execute the condition command
    handle_pipes(argv,argc1,argv_count);

    // int status;
    // pid_t pid = fork();
    // if (pid == 0)
    // {
    //     execvp(condition_argv[0], condition_argv);
    //     perror("execvp failed");
    //     exit(EXIT_FAILURE);
    // }
    // else if (pid > 0)
    // {
    //     waitpid(pid, &status, 0);
    // }
    // else
    // {
    //     perror("fork failed");
    //     return;
    // }
    
    // Check the condition command's exit status
    if (last_exit_status)
    {
        int condition_exit_status = last_exit_status;
        if (condition_exit_status == 0)
        {
            // Condition is true
            if (then_index != -1 && else_index != -1)
            {
                // 'then' and 'else' blocks both exist
                for (int i = then_index + 1; i < else_index; i++)
                {
                    // Execute the 'then' block
                    char *then_argv[MAX_ARG_COUNT];
                    int then_argc = 0;
                    while (argv1[i] != NULL && strcmp(argv1[i], "else") != 0)
                    {
                        then_argv[then_argc++] = argv1[i++];
                    }
                    then_argv[then_argc] = NULL;

                    // Create a temporary 3D array to pass to expand_commands
                    char ****temp_argv = (char ****)malloc(sizeof(char ***));
                    *temp_argv = (char ***)malloc(sizeof(char **));
                    (*temp_argv)[0] = then_argv;

                    int need_fork = 1;
                    expand_commands(temp_argv, &need_fork, &then_argc, then_argv[0]);
                    free(*temp_argv);
                    free(temp_argv);

                    if (need_fork == 0)
                    {
                        continue;
                    }

                    pid_t then_pid = fork();
                    if (then_pid == 0)
                    {
                        execvp(then_argv[0], then_argv);
                        perror("execvp failed");
                        exit(EXIT_FAILURE);
                    }
                    else if (then_pid > 0)
                    {
                        int then_status;
                        waitpid(then_pid, &then_status, 0);
                    }
                    else
                    {
                        perror("fork failed");
                    }
                }
            }
            else if (then_index != -1 && else_index == -1 && fi_index != -1)
            {
                // 'then' block only exists
                for (int i = then_index + 1; i < fi_index; i++)
                {
                    // Execute the 'then' block
                    char *then_argv[MAX_ARG_COUNT];
                    int then_argc = 0;
                    while (argv1[i] != NULL && strcmp(argv1[i], "fi") != 0)
                    {
                        then_argv[then_argc++] = argv1[i++];
                    }
                    then_argv[then_argc] = NULL;

                    // Create a temporary 3D array to pass to expand_commands
                    char ****temp_argv = (char ****)malloc(sizeof(char ***));
                    *temp_argv = (char ***)malloc(sizeof(char **));
                    (*temp_argv)[0] = then_argv;

                    int need_fork = 1;
                    expand_commands(temp_argv, &need_fork, &then_argc, then_argv[0]);
                    free(*temp_argv);
                    free(temp_argv);

                    if (need_fork == 0)
                    {
                        continue;
                    }

                    pid_t then_pid = fork();
                    if (then_pid == 0)
                    {
                        execvp(then_argv[0], then_argv);
                        perror("execvp failed");
                        exit(EXIT_FAILURE);
                    }
                    else if (then_pid > 0)
                    {
                        int then_status;
                        waitpid(then_pid, &then_status, 0);
                    }
                    else
                    {
                        perror("fork failed");
                    }
                }
            }
        }
        else if (else_index != -1 && fi_index != -1)
        {
            // 'else' block exists
            for (int i = else_index + 1; i < fi_index; i++)
            {
                // Execute the 'else' block
                char *else_argv[MAX_ARG_COUNT];
                int else_argc = 0;
                while (argv1[i] != NULL && strcmp(argv1[i], "fi") != 0)
                {
                    else_argv[else_argc++] = argv1[i++];
                }
                else_argv[else_argc] = NULL;

                // Create a temporary 3D array to pass to expand_commands
                char ****temp_argv = (char ****)malloc(sizeof(char ***));
                *temp_argv = (char ***)malloc(sizeof(char **));
                (*temp_argv)[0] = else_argv;

                int need_fork = 1;
                expand_commands(temp_argv, &need_fork, &else_argc, else_argv[0]);
                free(*temp_argv);
                free(temp_argv);

                if (need_fork == 0)
                {
                    continue;
                }

                pid_t else_pid = fork();
                if (else_pid == 0)
                {
                    execvp(else_argv[0], else_argv);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                }
                else if (else_pid > 0)
                {
                    int else_status;
                    waitpid(else_pid, &else_status, 0);
                }
                else
                {
                    perror("fork failed");
                }
            }
        }
    }
}

void expand_commands(char ****argv, int *need_fork, int *argc, char *command)
{
    char ***argvMat = *argv;

    // Check for the !! command
    if (strcmp(command, "!!") == 0)
    {
        if (strlen(last_command) == 0)
        {
            printf("No previous command to repeat.\n");
            *need_fork = 0;
            return;
        }
        strcpy(command, last_command);
        int num_subtokens;
        argvMat[0] = split_string(last_command, ' ', &num_subtokens);
    }
    else
    {
        strcpy(last_command, command); // Store the current command as the last command
    }
    strcpy(curr_command, command);

    // Check if the command is empty
    if (argvMat[0][0] == NULL)
    {
        *need_fork = 0;
        return;
    }

    int argc1 = argc[0];

    // Check for background execution
    if (argc1 > 0 && strcmp(argvMat[0][argc1 - 1], "&") == 0)
    {
        amper = 1;
        argvMat[0][argc1 - 1] = NULL;
    }
    else
    {
        amper = 0;
    }

    add_to_history(curr_command);

    // Check for output redirection
    if (argc1 > 2 && strcmp(argvMat[0][argc1 - 2], ">") == 0)
    {
        redirect_out = 1;
        argvMat[0][argc1 - 2] = NULL;
        outfile = argvMat[0][argc1 - 1];
    }
    else if (argc1 > 2 && strcmp(argvMat[0][argc1 - 2], "2>") == 0)
    {
        redirect_err = 1;
        argvMat[0][argc1 - 2] = NULL;
        errfile = argvMat[0][argc1 - 1];
    }
    else if (argc1 > 2 && strcmp(argvMat[0][argc1 - 2], ">>") == 0)
    {
        redirect_out_app = 1;
        argvMat[0][argc1 - 2] = NULL;
        outfile = argvMat[0][argc1 - 1];
    }
    else
    {
        redirect_out = 0;
        redirect_out_app = 0;
        redirect_err = 0;
    }

    // Check for built-in commands
    if (argc1 > 1 && strcmp(argvMat[0][0], "prompt") == 0)
    {
        free(prompt_name);
        prompt_name = malloc(strlen(argvMat[0][argc1 - 1]) + 1);
        if (prompt_name == NULL)
        {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        strcpy(prompt_name, argvMat[0][argc1 - 1]);
        *need_fork = 0;
    }
    else if (argc1 > 1 && strcmp(argvMat[0][0], "echo") == 0)
    {
        if (strcmp(argvMat[0][1], "$?") == 0)
        {
            printf("%d \n", last_exit_status);
        }
        else
        {
            // Check for variable substitution
            for (int i = 0; argvMat[0][i] != NULL; i++)
            {
                char *value = get_variable_value(argvMat[0][i]);
                if (value != NULL)
                {
                    argvMat[0][i] = value;
                }
            }

            for (int i = 1; i < argc1; i++)
            {
                printf("%s ", argvMat[0][i]);
            }
            printf("\n");
        }
        *need_fork = 0;
    }
    else if (argc1 > 1 && strcmp(argvMat[0][0], "cd") == 0)
    {
        if (chdir(argvMat[0][1]) != 0)
        {
            perror("chdir failed");
        }
        *need_fork = 0;
    }
    else if (argc1 == 1 && strcmp(argvMat[0][0], "quit") == 0)
    {
        exit(EXIT_SUCCESS);
    }
    else if (argc1 > 2 && argvMat[0][argc1 - 2] != NULL && strcmp(argvMat[0][argc1 - 2], "=") == 0)
    {
        set_variable_value(argvMat[0][argc1 - 3], argvMat[0][argc1 - 1]);
        *need_fork = 0;
    }
    else if (argc1 == 2 && strcmp(argvMat[0][0], "read") == 0)
    {
        char value[MAX_COMMAND_LENGTH];
        if (fgets(value, sizeof(value), stdin) == NULL)
        {
            perror("fgets failed");
            *need_fork = 0;
        }
        value[strcspn(value, "\n")] = '\0'; // Remove trailing newline
        // Add a $ before argv1[1] using strcat
        char var_name[MAX_COMMAND_LENGTH] = "$";
        strcat(var_name, argvMat[0][1]);

        set_variable_value(var_name, value);
        *need_fork = 0;
    }
}

int main()
{
    char ***argv;
    int  retid, status;
    int fd, fd_err;

    prompt_name = malloc(strlen("hello") + 1);
    if (prompt_name == NULL)
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    argv = (char ***)malloc(MAX_ARG_COUNT * sizeof(char **));
    if (argv == NULL)
    {
        fprintf(stderr, "Memory allocation failed for argv\n");
        return 1; // Return error code
    }
    argvAllocate(&argv);

    strcpy(prompt_name, "hello");

    // Save the original stderr file descriptor
    int original_stderr = dup(STDERR_FILENO);

    // Register the signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    while (1)
    {
        // Register the signal handler for SIGINT
        signal(SIGINT, handle_sigint);
        command[MAX_COMMAND_LENGTH - 1] = '\0'; // Remove trailing newline

        int num_tokens;
        int argc[MAX_SUBCOMMAND_COUNTER] = {0};
        int argv_count;
        int needfork = 1;

        read_input_with_history(command, prompt_name);

        parse_command(command, &argv, argc, &argv_count);

        // Check if the command is empty
        if (argv[0][0] == NULL)
            continue;

        
        // Handle if-else statements
        if (argc[0] > 0 && strcmp(argv[0][0], "if") == 0)
        {
            execute_if_else(command);
            
            needfork = 0;
        }

        // Expand commands
        expand_commands(&argv, &needfork, argc, command);

        

        if (needfork == 0)
        {
            continue;
        }

        // Handle the piping commands
        handle_pipes(argv, argc, argv_count);
    }

    // Close the original stderr file descriptor
    close(original_stderr);
    free(prompt_name);

    return 0;
}
