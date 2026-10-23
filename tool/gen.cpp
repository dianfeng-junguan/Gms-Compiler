/*
  this code is used to auto-generate the debugging-needed part like
  token_type_tostr function, which need an array mapping the enum and string.
  compile and run this before actually building the project.
  the program will generate a separate file including the definition of the
  arrays
*/
#include <assert.h>
#include <ctype.h>
#include <regex>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

// --- Configuration Structure for Unified Interface ---

/**
 * Configuration struct to define an enum generation job.
 * Add more entries to the array in main() to handle other enums.
 */
typedef struct {
  const char *filename;      // The header file to read (e.g., "lexer.h")
  const char *enum_name;     // The enum type name (e.g., "token_type_t")
  const char *function_name; // The output function name (e.g., "token_type_tostr")
} EnumConfig;

// --- Helper Functions ---

/**
 * Reads the entire file content into a malloc'd string.
 */
char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Failed to open file %s\n", path);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long f_len = ftell(f);
  rewind(f);

  char *code = (char *)malloc(f_len + 1);
  assert(code);
  if (fread(code, f_len, 1, f) != 1 && f_len > 0) {
    fprintf(stderr, "Error: Failed to read file %s\n", path);
    fclose(f);
    free(code);
    return NULL;
  }
  code[f_len] = '\0';
  fclose(f);
  return code;
}

/**
 * Trims leading and trailing whitespace from a string.
 * Modifies the string in-place.
 */
char *trim_whitespace(char *str) {
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str))
    str++;

  if (*str == 0) // All spaces
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

/**
 * Cleans a raw enum member token.
 * Removes comments (//), assignments (= value), and whitespace.
 * Returns the cleaned identifier or NULL if empty.
 */
char *clean_enum_member(char *raw_token) {
  // 1. Remove comments
  do{
    char *comment = strstr(raw_token, "//");
    if (comment) {
      char *comment_end = comment;
      while (*comment_end&&*(comment_end++)!='\n') {
      }
      size_t restlen = strlen(comment_end);
      memmove(comment, comment_end, restlen);
      *(comment+restlen)='\0';
    }else{
      break;
    }
  }while(1);
    
  // Note: This simple parser doesn't handle /* block comments */ inside lines well,
  // assuming standard one-line enum definitions.

  // 2. Remove assignments (e.g., IF = 10)
  char *assignment = strchr(raw_token, '=');
  if (assignment)
    *assignment = '\0';

  // 3. Trim whitespace
  char *cleaned = trim_whitespace(raw_token);

  if (strlen(cleaned) == 0)
    return NULL;

  return cleaned;
}

// --- Core Generator Logic ---

/**
 * Generates the C code for the enum-to-string function.
 */
void generate_enum_mapper(char *file_content, EnumConfig config) {
  char pattern[512];

  // Construct regex pattern dynamically to find specific enum
  // Pattern: typedef [whitespace] enum [whitespace] { (capture body) } [whitespace] EnumName;
  // We use [^;]* to capture the body broadly until the closing brace logic,
  // but a greedy capture with specific anchor is safer.
  // NOTE: ((.|[\r\n])*) captures multi-line content.
  snprintf(pattern, sizeof(pattern), 
	   "typedef\\s+enum\\s*\\{([\\s\\S]*)\\}\\s*%s;", 
	   config.enum_name);

  try {
    std::regex reg(pattern);
    std::cmatch matches;

    if (std::regex_search(file_content, matches, reg)) {
      // Extract the body (content inside {})
      std::string body_string = matches[1].str();
      int len = (int)body_string.length();
        
      char *body = (char *)malloc(len + 1);
      memcpy(body, body_string.c_str(), len);
      body[len] = '\0';

      // Output the function definition
      printf("\n// Auto-generated function for %s\n", config.enum_name);
      printf("const char* %s(%s type) {\n", config.function_name, config.enum_name);
      printf("  switch(type) {\n");

      // Tokenize by comma
      char *cursor = body;
      char *token = strtok(cursor, ",\n");
      while (token != NULL) {
        char *member = clean_enum_member(token);
            
        if (member) {
	  printf("    case %s: return \"%s\";\n", member, member);
        }
            
        token = strtok(NULL, ",");
      }

      printf("    default: return \"UNKNOWN_%s\";\n", config.enum_name);
      printf("  }\n");
      printf("}\n");

      free(body);
    } else {
      fprintf(stderr, "Warning: Could not find definition for enum '%s' in %s\n", 
              config.enum_name, config.filename);
    }
  } catch (const std::regex_error &) {
    fprintf(stderr, "Error: Failed to compile regex for %s\n", config.enum_name);
  }
}

// --- Main Driver ---

int main() {
  // 1. Define the jobs (Unified Interface)
  EnumConfig jobs[] = {
    {.filename = "lexer.h",
     .enum_name = "token_type_t",
     .function_name = "token_type_tostr"},
    {.filename = "parser.h",
     .enum_name = "astnode_type_t",
     .function_name = "get_nodetype_str"},

    {.filename = "intercode.h",
     .enum_name = "intercode_type_t",
     .function_name = "codetype_tostr"},      
    // You can add more enums here easily:
    // { "parser.h", "astnode_type_t", "astnode_type_tostr" },
  };

  size_t job_count = sizeof(jobs) / sizeof(EnumConfig);

  printf("/* \n * AUTO-GENERATED FILE. DO NOT EDIT.\n * Generated by gen_enum.c \n */\n");
  printf("#include \"lexer.h\"\n"); // Include necessary headers
  printf("#include \"parser.h\"\n");
  printf("#include \"intercode.h\"\n");
  // printf("#include \"parser.h\"\n"); 

  // 2. Process each job
  for (size_t i = 0; i < job_count; ++i) {
    char *content = read_file(jobs[i].filename);
    if (content) {
      generate_enum_mapper(content, jobs[i]);
      free(content);
    }
  }

  return 0;
}
