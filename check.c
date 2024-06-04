#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function to trim leading and trailing spaces
char *trim(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str; // All spaces?

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = 0;

    return str;
}

// Function to split a string by a delimiter and handle multiple spaces
char **split_string(const char *str, const char delimiter, int *num_tokens) {
    int count = 0;
    const char *temp = str;

    // Count the number of delimiters
    while (*temp) {
        if (*temp == delimiter) count++;
        temp++;
    }
    
    // Allocate memory for tokens
    char **tokens = malloc((count + 1) * sizeof(char *));
    if (tokens == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    int index = 0;
    char *start = strdup(str);
    char *end = strchr(start, delimiter);
    while (end != NULL) {
        *end = '\0';
        tokens[index++] = trim(start);
        start = end + 1;
        end = strchr(start, delimiter);
    }
    tokens[index++] = trim(start);

    *num_tokens = index;
    return tokens;
}

int main() {
    char command[] = "bar -c | alayof -d | p -c | t -r";

    int num_tokens;
    char **commands = split_string(command, '|', &num_tokens);

    for (int i = 0; i < num_tokens; i++) {
        printf("Command %d: %s\n", i + 1, commands[i]);

        // Tokenize each command by spaces
        int num_subtokens;
        char **subtokens = split_string(commands[i], ' ', &num_subtokens);

        for (int j = 0; j < num_subtokens; j++) {
            printf("  Subtoken %d: %s\n", j + 1, subtokens[j]);
        }

        free(subtokens); // Free the memory allocated for subtokens
    }

    free(commands); // Free the memory allocated for commands

    return 0;
}
