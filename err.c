#include "utils.h"
#include <stdio.h>
#include "err.h"


const char *SENDER_LEXER = "lexer";
const char *SENDER_PARSER = "parser";
const char *SENDER_SEMATIC = "sematic";
const char *SENDER_INTERCODER = "intercode generator";
const char *SENDER_ASMGEN = "assembly generator";

void cry_error(const char* sender, char* msg, filepos_t pos){
  printf("Error from %s:%s at line %zu, column %zu\n",sender, msg, pos.line, pos.column);
}
