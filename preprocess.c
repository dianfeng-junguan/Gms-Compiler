#include "err.h"
#include "utils.h"
#include "lexer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocess.h"
bool preprocess_tokens(list_t* tokens, char* source_dir){
  // preprocess include
  for (size_t i=0; i<tokens->len; i++) {
    token_t *tok = list_get(tokens, i);
    if (tok->token_type==INCLUDE) {
      token_t *pathtok = list_get(tokens, i + 1);
      if (!pathtok) {
        cry_error(SENDER_LEXER, "missing path after include\n", (filepos_t){0});
	return false;
      }
      if (pathtok->token_type!=CONSTANT_STRING) {
        cry_error(SENDER_LEXER, "invalid path string after include\n",
                  (filepos_t){0});
	return false;
      }
      cstring_t path = string_from(pathtok->value);
      memmove(path.data, path.data + 1, path.len - 2);
      path.data[path.len - 2] = '\0';
      char *full_path = malloc(strlen(source_dir) + path.len - 2 + 1);
      strcpy(full_path, source_dir);
      strcat(full_path, path.data);      
      FILE *f = fopen(full_path, "r");
      free_string(&path);
      free(full_path);
      if (!f) {
        perror("include failed");
        return false;
      }
      fseek(f, 0, SEEK_END);
      size_t len=ftell(f);
      fseek(f, 0, SEEK_SET);
      char* source=malloc(len+1);
      fread(source, len, 1, f);
      source[len]='\0';
      fclose(f);
      list_t newtoks = do_lex(source);
      free(source);
      // insert
      list_remove(tokens, i);      
      list_remove(tokens, i);
      for (size_t j=0; j<newtoks.len; j++) {
	list_insert(tokens, i+j, list_get(&newtoks, j));
      }
      free_list(&newtoks);
      i++;
    }
  }
  return true;  
}
