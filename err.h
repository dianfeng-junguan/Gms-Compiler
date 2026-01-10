#ifndef _ERR_H
#define _ERR_H
#include <stdio.h>
struct filepos_t;


extern const char *SENDER_LEXER;
extern const char *SENDER_PARSER;
extern const char *SENDER_SEMATIC;
extern const char *SENDER_INTERCODER;
extern const char *SENDER_ASMGEN;

/// cry out an error.
/// sender: values available: SENDER_LEXER, SENDER_PARSER, SENDER_SEMATIC,
/// SENDER_INTERCODER, SENDER_ASMGEN
void cry_error(const char* sender, char* msg, filepos_t pos);

#define cry_errorf(sender, pos, fmt, ...)	\
  do{						\
    printf("Error from %s:", sender);						\
    printf(fmt, ##__VA_ARGS__);						\
    printf(" at line %zu, column %zu\n", pos.line, pos.column); \
 }while(0);

#define panic(msg,...) \
  do {						\
    printf(msg, ##__VA_ARGS__);			\
    while(1);					\
  } while (0);					
#endif
