#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */

#define MAX_FILE_NAME_LENGTH 8192
#define MAX_LINE_LENGTH 8192

typedef int boolean_t;
#define true ((boolean_t) 1)
#define false ((boolean_t) 0)

typedef struct {
  boolean_t is_end_of_file;
  boolean_t overflow;
  int n_read;
} readline_result_t;

readline_result_t readline(char* output_buffer, FILE* input,
                           int size_of_output_buffer,
                           boolean_t include_newline);
boolean_t string_ends_with(const char* str1, const char* str2);
boolean_t string_starts_with(const char* str1, const char* str2);

/* ====================================================================== */

/** Header files at the top of a C file look like this:
 *
 *#ifndef _FOOBAR_H_
 *   **NORMAL HEADER FILE STUFF **
 * #endif /* _FOOBAR_H_ *\/
 *
 * So we just need to extract such blocks into a header file
 * (including the guard).
 *
 * returns true when a header file was created.
 */
boolean_t process_file(char* input_file_name) {

  boolean_t created = 0;

  char output_file_name[MAX_FILE_NAME_LENGTH];
  strcpy(output_file_name, input_file_name);
  int file_name_length = strlen(input_file_name);
  output_file_name[file_name_length - 1] = 'h';

  char line[MAX_LINE_LENGTH];
  int line_size = MAX_LINE_LENGTH;

  FILE* input_file = fopen(input_file_name, "r");
  if (input_file == NULL) {
    fprintf(stderr, "ERROR: Couldn't open '%s'\n", input_file_name);
    exit(-1);
  }

  while (1) {
    readline_result_t status
        = readline(line, input_file, MAX_LINE_LENGTH, true);
    if (status.overflow) {
      fprintf(stderr, "ERROR: Max line length exceeded in file %s\n",
              input_file_name);
      exit(-1);
    } else if (status.is_end_of_file) {
      break;
    }

    if (string_starts_with(line, "#ifndef _")
        && string_ends_with(line, "_H_\n")) {
      created = 1;
      // open file_name for output
      FILE* output_file = fopen(output_file_name, "w");
      if (output_file == NULL) {
        fprintf(stderr, "ERROR: Couldn't open output file %s\n",
                output_file_name);
        fclose(input_file);
        exit(-1);
      }

      // write the line we just read
      fprintf(output_file, "%s", line);

      while (1) {

        readline_result_t status
            = readline(line, input_file, MAX_LINE_LENGTH, true);
        if (status.overflow) {
          fprintf(stderr, "ERROR: Max line length exceeded in file %s\n",
                  input_file_name);
          exit(-1);
        } else if (status.is_end_of_file) {
          fprintf(stderr,
                  "ERROR: input ended before header file marker match '%s'\n",
                  input_file_name);
        }

        // write the line we just read to the output file we opened
        fprintf(output_file, "%s", line);

        if (string_starts_with(line, "#endif /* _")
            && string_ends_with(line, "_H_ */\n")) {
          break;
        }
      }

      fclose(output_file);
    }
  }

  fclose(input_file);
  return created;
}

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    process_file(argv[i]);
  }
  return 0;
}

boolean_t string_starts_with(const char* str1, const char* str2) {
  return strncmp(str1, str2, strlen(str2)) == 0;
}

boolean_t string_ends_with(const char* str1, const char* str2) {
  size_t len1 = strlen(str1);
  size_t len2 = strlen(str2);

  if (len2 > len1) {
    return 0;
  }

  return strcmp(str1 + (len1 - len2), str2) == 0;
}

readline_result_t readline(char* output_buffer, FILE* input,
                           int size_of_output_buffer,
                           boolean_t include_newline) {
  readline_result_t result = {0};

  int n_bytes = 0;

  while (1) {
    char c = fgetc(input);

    if (c == EOF) {
      result.is_end_of_file = true;
      break;
    }

    if (n_bytes >= size_of_output_buffer - 1) {
      result.overflow = true;
      break;
    }

    output_buffer[n_bytes] = c;
    if (c == '\n' && !include_newline) {
      output_buffer[n_bytes] = '\0';
      break;
    } else if (c == '\n') {
      n_bytes++;
      output_buffer[n_bytes] = '\0';
      break;
    }

    n_bytes++;
  }

  result.n_read = n_bytes;

  return result;
}
