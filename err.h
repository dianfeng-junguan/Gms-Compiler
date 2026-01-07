#ifndef _ERR_H
#define _ERR_H
#include <stdio.h>
struct filepos_t;


const char *SENDER_LEXER = "lexer";
const char *SENDER_PARSER = "parser";
const char *SENDER_SEMATIC = "sematic";
const char *SENDER_INTERCODER = "intercode generator";
const char *SENDER_ASMGEN = "assembly generator";

/// cry out an error.
/// sender: values available: SENDER_LEXER, SENDER_PARSER, SENDER_SEMATIC,
/// SENDER_INTERCODER, SENDER_ASMGEN
void cry_error(const char* sender, char* msg, filepos_t pos);

#define panic(msg,...) \
  do {						\
    printf(msg, ##__VA_ARGS);			\
    while(1);					\
  } while (0);					
#endif
