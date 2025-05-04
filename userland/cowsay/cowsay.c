#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_WIDTH 40  // Maximum characters per line before wrapping


static char *merge_args(int argc, char *argv[]) {
    size_t total_len = 0;
    for (int i = 1; i < argc; i++) {
        total_len += strlen(argv[i]) + 1;  // +1 for space
    }

    char *merged = malloc(total_len + 1);
    if (!merged) {
        perror("malloc");
        exit(1);
    }
    merged[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(merged, argv[i]);
        if (i < argc - 1) strcat(merged, " ");
    }
    return merged;
}

static char **wrap_text(const char *text, int *line_count) {
    // Duplicate text for tokenization
    char *dup = strdup(text);
    if (!dup) {
        perror("strdup");
        exit(1);
    }

    char **lines = NULL;
    int lines_alloc = 0;
    *line_count = 0;

    char *word = strtok(dup, " ");
    char *line = malloc(MAX_WIDTH + 1);
    if (!line) {
        perror("malloc");
        exit(1);
    }
    line[0] = '\0';

    while (word) {
        int word_len = strlen(word);
        int line_len = strlen(line);

        if (line_len + word_len + (line_len > 0 ? 1 : 0) > MAX_WIDTH) {
            // Save current line
            if (*line_count >= lines_alloc) {
                lines_alloc = lines_alloc ? lines_alloc * 2 : 4;
                lines = realloc(lines, lines_alloc * sizeof(char *));
                if (!lines) {
                    perror("realloc");
                    exit(1);
                }
            }
            lines[*line_count] = strdup(line);
            (*line_count)++;
            line[0] = '\0';
            line_len = 0;
        }

        if (line_len > 0)
            strcat(line, " ");
        strcat(line, word);

        word = strtok(NULL, " ");
    }

    // Add last line
    if (strlen(line) > 0) {
        if (*line_count >= lines_alloc) {
            lines_alloc++;
            lines = realloc(lines, lines_alloc * sizeof(char *));
            if (!lines) {
                perror("realloc");
                exit(1);
            }
        }
        lines[*line_count] = strdup(line);
        (*line_count)++;
    }

    free(line);
    free(dup);
    return lines;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        return 1;
    }

    char *message = merge_args(argc, argv);

    int lines_count;
    char **lines = wrap_text(message, &lines_count);

    // Determine the maximum line length
    int max_len = 0;
    for (int i = 0; i < lines_count; i++) {
        int len = strlen(lines[i]);
        if (len > max_len)
            max_len = len;
    }

    // Top border
    printf(" ");
    for (int i = 0; i < max_len + 2; i++)
        printf("_");
    printf("\n");

    for (int i = 0; i < lines_count; i++) {
        char leftborder = lines_count == 1 ? '<' :
            i == 0 ? '/' :
            i == lines_count - 1 ? '\\' : '|'; 
        printf("%c %s", leftborder, lines[i]);
        
        // Padding
        for (int j = strlen(lines[i]); j < max_len; j++)
            printf(" ");
        
        char rightborder = lines_count == 1 ? '>' :
            i == 0 ? '\\' :
            i == lines_count - 1 ? '/' : '|';

        printf(" %c\n", rightborder);
    }

    // Bottom border
    printf(" ");
    for (int i = 0; i < max_len + 2; i++)
        printf("-");
    printf("\n");

    printf("        \\   ^__^\n");
    printf("         \\  (oo)\\_______\n");
    printf("            (__)\\       )/\\/\\\n");
    printf("                ||----w |");
    printf("\n");
    printf("                ||     ||\n");

    return 0;
}
