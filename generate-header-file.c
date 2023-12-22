#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ====================================================================== */

#define MAX_FILE_NAME_LENGTH 8192
#define MAX_LINE_LENGTH 8192
#define GENERATED_FILE_TAG "// SSCF generated file from:"
#define ADD_LINE_DIRECTIVE 1

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
boolean_t file_exists(const char* path);

/* ====================================================================== */

typedef enum {
  NO_HEADER_BLOCK,
  HEADER_BLOCK_MATCHES_CURRENT_HEADER_FILE,
  HEADER_FILE_WRITTEN,
  ERROR_NOT_OVERWRITING_NON_AUTO_GENERATED_HEADER_FILE,
} process_file_result_t;

process_file_result_t maybe_move_file(char* real_name, char* tmp_name);
boolean_t is_generated_file(char* name);
int file_contents_identical(const char* file1, const char* file2);

/**
 * Header files at the top of a C file must look like this:
 *
 * #ifndef _FOOBAR_H_
 * #define _FOOBAR_H_
 *   **NORMAL HEADER FILE STUFF **
 * #endif /* _FOOBAR_H_ *\/
 *
 * So we just need to extract such a block into a header file
 * (including the guard).
 *
 * This tool won't overwrite a header file we don't "know" was
 * automatically generated and no changes are made if the header file
 * wouldn't change so we can keep build systems happy.
 */
process_file_result_t process_file(char* input_file_name) {

  // TODO(jawilson): make sure we don't overflow...

  char output_file_name[MAX_FILE_NAME_LENGTH];
  strcpy(output_file_name, input_file_name);
  int file_name_length = strlen(input_file_name);
  output_file_name[file_name_length - 1] = 'h';

  char tmp_output_file_name[MAX_FILE_NAME_LENGTH];
  strcpy(tmp_output_file_name, output_file_name);
  strcat(tmp_output_file_name, ".tmp");

  // Now start the real work.

  process_file_result_t status_result = NO_HEADER_BLOCK;

  char line[MAX_LINE_LENGTH];
  int line_size = MAX_LINE_LENGTH;

  FILE* input_file = fopen(input_file_name, "r");
  if (input_file == NULL) {
    fprintf(stderr, "ERROR: Couldn't open '%s'\n", input_file_name);
    exit(-1);
  }

  int c_file_line_number = 0;

  while (1) {
    readline_result_t status
        = readline(line, input_file, MAX_LINE_LENGTH, true);
    c_file_line_number++;

    if (status.overflow) {
      fprintf(stderr, "ERROR: Max line length exceeded in file %s\n",
              input_file_name);
      exit(-1);
    } else if (status.is_end_of_file) {
      break;
    }

    if (string_starts_with(line, "#ifndef _")
        && string_ends_with(line, "_H_\n")) {

      // open a temporary file_name for output
      FILE* output_file = fopen(tmp_output_file_name, "w");
      if (output_file == NULL) {
        fprintf(stderr, "ERROR: Couldn't open output file %s\n",
                tmp_output_file_name);
        fclose(input_file);
        exit(-1);
      }

      fprintf(output_file, "%s %s\n\n", GENERATED_FILE_TAG, input_file_name);
#ifdef ADD_LINE_DIRECTIVE 
      fprintf(output_file, "#line %d \"%s\"\n", c_file_line_number, input_file_name);
#endif

      // write the line we just read
      fprintf(output_file, "%s", line);

      while (1) {
        readline_result_t status
            = readline(line, input_file, MAX_LINE_LENGTH, true);
        c_file_line_number++;
        if (status.overflow) {
          fprintf(stderr, "ERROR: Max line length exceeded in file %s\n",
                  input_file_name);
          exit(-1);
        } else if (status.is_end_of_file) {
          fprintf(stderr,
                  "ERROR: input ended before header file marker match '%s'\n",
                  input_file_name);
          exit(-1);
        }

        fprintf(output_file, "%s", line);

        if (string_starts_with(line, "#endif /* _")
            && string_ends_with(line, "_H_ */\n")) {
          break;
        }
      }
      fclose(output_file);
      status_result = maybe_move_file(output_file_name, tmp_output_file_name);
      break;
    }
  }

  fclose(input_file);

  switch (status_result) {
  case NO_HEADER_BLOCK:
    fprintf(stderr, "no header block for %s\n", input_file_name);
    break;
  case HEADER_BLOCK_MATCHES_CURRENT_HEADER_FILE:
    fprintf(stderr, "header file %s is already up to date.\n",
            output_file_name);
    break;
  case ERROR_NOT_OVERWRITING_NON_AUTO_GENERATED_HEADER_FILE:
    fprintf(stderr, "ERROR not overwriting existing file %s\n",
            output_file_name);
    fprintf(stderr, "Is it possible this file was written by hand?\n");
    exit(1);
    break;
  case HEADER_FILE_WRITTEN:
    fprintf(stderr, "extracted header file to %s\n", output_file_name);
    break;
  }

  return status_result;
}

/**
 * Create or update the header file with the tmp file as long as they
 * aren't identical content or if the header file looks like it was
 * authored by hand.
 */
process_file_result_t maybe_move_file(char* real_name, char* tmp_name) {
  if (file_exists(real_name)) {
    if (file_contents_identical(real_name, tmp_name)) {
      unlink(tmp_name);
      return HEADER_BLOCK_MATCHES_CURRENT_HEADER_FILE;
    }
    if (!is_generated_file(real_name)) {
      return ERROR_NOT_OVERWRITING_NON_AUTO_GENERATED_HEADER_FILE;
    }
  }
  unlink(real_name);
  rename(tmp_name, real_name);
  return HEADER_FILE_WRITTEN;
}

/**
 * Look at the first line of a file to see if it is probably one of
 * our generated files...
 */
boolean_t is_generated_file(char* name) {
  char line[MAX_LINE_LENGTH];
  FILE* input = fopen(name, "r");
  if (input == NULL) {
    fprintf(stderr, "ERROR: internal error. Exiting.");
    exit(-1);
  }
  readline(line, input, MAX_LINE_LENGTH, true);
  fclose(input);
  if (string_starts_with(line, GENERATED_FILE_TAG)) {
    return true;
  }
  return false;
}

void help() {
  fprintf(stderr, "generate-header-file <c-file-1> <c-file-2> ...\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Source files are never modified. Header file names are\n");
  fprintf(stderr, "Determined from each source file.\n");
}

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "help") == 0) {
      help();
      exit(0);
    }
    if (string_ends_with(argv[i], ".h")
        || string_ends_with(argv[i], ".hpp")) {
      fprintf(stderr, "ERROR: filename ends with .h (or .hpp) - should give input file\n\n");
      help();
      exit(1);
    }
    process_file(argv[i]);
  }
  return 0;
}

// The remaining code is really a small library.

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

/**
 * A sane replacement for getline and other ways of reading a string.
 *
 * Note that n_read may not be equal to strlen() specifically if
 * include_newline is false.
 */
readline_result_t readline(char* output_buffer, FILE* input,
                           int size_of_output_buffer,
                           boolean_t include_newline) {
  output_buffer[0] = '\0';
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

// TODO(jawilson): read more than a byte at a time?
int file_contents_identical(const char* file1, const char* file2) {
  FILE* f1 = fopen(file1, "r");
  FILE* f2 = fopen(file2, "r");

  if (f1 == NULL || f2 == NULL) {
    fprintf(stderr, "one of the files doesn't exist!");
    exit(-1);
  }

  char c1 = EOF;
  char c2 = EOF;

  while (1) {
    c1 = fgetc(f1);
    c2 = fgetc(f2);

    if (c1 == EOF || c2 == EOF) {
      break;
    }
  }

  fclose(f1);
  fclose(f2);

  if (c1 != EOF || c2 != EOF) {
    fprintf(stderr, "Generated files are not the same length?\n");
    return 0;
  }

  return 1;
}

boolean_t file_exists(const char* path) { return access(path, F_OK) == 0; }
