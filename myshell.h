#ifndef SHELL_H
#define SHELL_H

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

void disable_raw_mode();
void enable_raw_mode();
void handle_sigint();
void print_status();
char *trim(char *str);
char *my_strdup(const char *s);
char **split_string(const char *str, const char delimiter, int *num_tokens);
void argvAllocate(char ****argv);
void parse_command(char *command, char ****argv, int *argc, int *argv_count);
char *get_variable_value(const char *name);
void set_variable_value(const char *name, const char *value);
void add_to_history(const char *command);
void display_command_from_history(char *command, const char *prompt_name);
void handle_arrow_key_press(int key, char *command, const char *prompt_name);
void read_input_with_history(char *command, const char *prompt_name);
void handle_pipes(char ***argv, int argv_count);
void execute_if_else(char *command);
void expand_commands(char ****argv, int *need_fork, int *argc, char *command);

#define MAX_ARG_COUNT 10          // max pipes
#define MAX_COMMAND_LENGTH 1024   // command length
#define MAX_SUBCOMMAND_LENGTH 480 // subcommand length
#define MAX_SUBCOMMAND_COUNTER 10 // subcommand counter
#define MAX_HISTORY_SIZE 20
#define UP_ARROW 65
#define DOWN_ARROW 66
#define ESCAPE_KEY 27
#define BACKSPACE 127

#endif // SHELL_H