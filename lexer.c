#include "lexer.h"
#include "err.h"
#include "stdbool.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool string_begin(char c) { return c == '\"'; }
static bool met_quote = false;
bool string_allowed(char c, char *scanned, size_t offset) {
  if (c == '\"' && !met_quote) {
    met_quote = true;
    return true;
  }
  if (met_quote) {
    met_quote = false;
    return false;
  }
  return c != '\"';
}
bool string_after(char *str, list_t *tokens, filepos_t pos) {
  token_t tok = create_token(CONSTANT_STRING, str, pos);
  list_append(tokens, &tok);
  return true;

}
bool char_begin(char c) { return c == '\''; }
static bool met_singlequote = false;
bool char_allowed(char c, char *scanned, size_t offset) {
  if (c == '\'' && !met_singlequote) {
    met_singlequote = true;
    return true;
  }
  if (met_singlequote) {
    met_singlequote = false;
    return false;
  }
  return c != '\'';
}
bool char_after(char *str, list_t *tokens, filepos_t pos) {
  if(strlen(str)>3){
    // two quote and a character (not considering \n etc for now)
    return false;
  }
  token_t tok = create_token(CONSTANT_CHAR, str, pos);
  list_append(tokens, &tok);
  return true;
}

bool number_begin(char c) { return isdigit(c); }
bool number_allowed(char c, char *scanned, size_t offset) {
  return ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F') || isdigit(c) ||
         c == 'x' || c == 'X' || c == 'h' || c == 'H';
}
bool number_after(char *str, list_t *tokens, filepos_t pos) {
  // basically we just check spcial beginnings like 0x, 0b, 0o and ending h.
  size_t len = strlen(str);

  int hex_mode = 0;
  if (strncmp(str, "0x", 2) == 0) {
    // 0x mode
    hex_mode = 1;
  } else if (str[len - 1] == 'h') {
    // h mode
    hex_mode = 2;
  }
  // a flag for final check
  bool flag = true;
  if (hex_mode > 0) {
    // hex number
    // we check middle chars
    bool flag = true;
    size_t beg = hex_mode == 1 ? 2 : 0;
    size_t end = hex_mode == 1 ? len : len - 1;
    for (size_t i = beg; i < end; i++) {
      if (!('a' <= str[i] && str[i] <= 'f' && 'A' <= str[i] && str[i] <= 'F' &&
            isdigit(str[i]))) {
        // invalid char
        cry_error(SENDER_LEXER,
                  "invalid hex number. A hex number should either begin with "
                  "0x or end with h.",
                  pos);
        flag = false;
        break;
      }
    }
  } else {
    // decimal.

    for (size_t i = 0; i < len; i++) {
      if (!isdigit(str[i])) {
        cry_error(SENDER_LEXER, "invalid decimal number.", pos);
        flag = false;
        break;
      }
    }
  }

  if (flag) {
    // create the token
    LOG(VERBOSE, "Creating number token:%s\n", str);
    token_t tok = create_token(CONSTANT_NUMBER, str, pos);
    list_append(tokens, &tok);
    return true;
  } else {
    return false;
  }
  // todo: other radixes
}

bool word_begin(char c) { return isalpha(c) || c == '_'; }
bool word_allowed(char c, char *start, size_t offset) {
  return isalpha(c) || c == '_';
}
typedef struct {
  char *str;
  tokentype_t tok;
} str_tok_pair_t;
str_tok_pair_t keywords[] = {
    {"fn", FN},         {"if", IF},         {"else", ELSE},
    {"while", WHILE},   {"return", RETURN}, {"let", LET},
    {"extern", EXTERN}, {"break", BREAK},

    {"int", INT},       {"string", STRING},
};
bool word_after(char *str, list_t *tokens, filepos_t pos) {
  // check if it is keyword
  token_t tok;
  bool f=false;
  for (size_t i = 0; i < sizeof(keywords) / sizeof(str_tok_pair_t); ++i) {
    if (strcmp(str, keywords[i].str) == 0) {
      tok = create_token(keywords[i].tok, str, pos);
      f=true;
    }
  }
  if (!f) {
    // identifier
    tok = create_token(IDENTIFIER, str, pos);
  }
  list_append(tokens, &tok);
  return true;
}

bool whitespace_begin(char c) { return isspace(c); }
bool whitespace_allowed(char c, char *start, size_t offset) {
  return isspace(c);
}
bool whitespace_after(char *str, list_t *tokens, filepos_t pos) {
  // we do not want the whitespace so we do nothing
  return true;
}
static str_op_pair_t operators[] = {
    {"==", EQUAL},      {">=", GREATER_EQUAL},
    {"<=", LESS_EQUAL}, {"!=", NOT_EQUAL},

    {"&&", AND},        {"||", OR},
    {"!", NOT},         {"^", XOR},

    {"+", ADD},         {"-", SUB},
    {"*", MUL},         {"/", DIV},
    {"%", MOD},

    {"<", GREATER},     {">", LESS},

    {"=", ASSIGN},

};

bool operator_begin(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^' ||
         c == '!' || c == '&' || c == '|' || c == '=' || c == '<' || c == '>' ||
         c == '|' || c == '&';
}
bool operator_allowed(char c, char *start, size_t offset) {
  if (offset > 1) {
    // no operator is longer than 2 chars
    return false;
  }
  if (offset == 0) {
    // first char. we return true as long as it can be found in the array.
    char ops[2] = {c, '\0'};
    for (size_t i = 0; i < sizeof(operators) / sizeof(str_op_pair_t); ++i) {
      if (strcmp(ops, operators[i].str) == 0) {
        return true;
      }
    }
    return false;
  } else {
    // offset==1. second char. check if we can make a two-char operator.
    char ops[3] = {start[0], c, '\0'};
    //
    for (size_t i = 0; i < sizeof(operators) / sizeof(str_op_pair_t); ++i) {
      if (strcmp(ops, operators[i].str) == 0) {
        // match
        return true;
      }
    }
    return false;
  }
}
bool operator_after(char *str, list_t *tokens, filepos_t pos) {
  for (size_t i = 0; i < sizeof(operators); ++i) {
    if (strcmp(str, operators[i].str) == 0) {
      // match long op first
      token_t tok = create_token(operators[i].op, str, pos);
      list_append(tokens, &tok);
      return true;
    }
  }
  return false;
}

bool separator_begin(char c) {
  return c == ';' || c == ':' || c == '{' || c == '}' ||
         c == '(' || c == ')' || c == ',';
}
bool separator_allowed(char c, char *start, size_t offset) {
  if (offset >= 1) {
    // we want single-char separator
    return false;
  }
  return c == ';' || c == ':' || c == '{' || c == '}' ||
         c == '(' || c == ')' || c == ',';
}
static str_sep_pair_t separators[] = {
    {";", SEMICOLON}, {":", COLON},      {"{", OPENBRACE}, {"}", CLOSEBRACE},
    {"(", OPENPAREN}, {")", CLOSEPAREN}, {",", COMMA}};
bool separator_after(char *str, list_t *tokens, filepos_t pos) {
  // separator is easy.
  for (size_t i = 0; i < sizeof(separators) / sizeof(str_sep_pair_t); ++i) {
    if (strcmp(str, separators[i].str) == 0) {
      token_t tok = create_token(separators[i].sep, str, pos);
      list_append(tokens, &tok);
      return true;
    }
  }
  return false;
}
lex_recipe_t scan_recipe[] = {
    {number_begin, number_allowed, number_after}, // costant: number,
    {string_begin, string_allowed, string_after}, // costant: string,
    {char_begin, char_allowed, char_after}, // costant: char,
    {word_begin, word_allowed, word_after},       // id and keywords,
    {whitespace_begin, whitespace_allowed, whitespace_after}, // whitespace,
    {operator_begin, operator_allowed, operator_after},       // operator,
    {separator_begin, separator_allowed, separator_after},    // separator,

};
/*
  move forward the str ptr counting the line and column
 */
static void forward(char *str, size_t *ptr, size_t len, size_t *line,
                    size_t *col) {
  size_t c = 0;
  while (c < len && str[*ptr]) {
    (*col)++;
    if (str[*ptr] == '\n') {
      (*line)++;
      *col = 0;
    }
    (*ptr)++;
    c++;
  }
}
list_t do_lex(char *str) {
  size_t ptr = 0;
  size_t len = strlen(str);
  size_t line = 0;
  size_t col = 0;
  list_t tokens = create_list(100, sizeof(token_t));
  while (ptr < len) {
    // todo :
    //  scan by a list
    /*
      {
      {
        allow_number_begin,
        number_allowed
      },...

      }
     */
    bool token_flag = false;
    for (size_t i = 0; i < sizeof(scan_recipe) / sizeof(lex_recipe_t); ++i) {
      if (scan_recipe[i].begin(str[ptr])) {
        LOG(VERBOSE, "begin with recipe %zu, ptr=%zu\n", i, ptr);
        // beginning allowed
        size_t pioneer = ptr + 1;
        // scan the whole thing
        while (pioneer < len &&
               scan_recipe[i].scan(str[pioneer], &str[ptr], pioneer - ptr)) {
          pioneer++;
        }
        LOG(VERBOSE, "scanned, pioneer=%zu\n", pioneer);
        if (pioneer == ptr) {
          // empty str?
          LOG(VERBOSE,
              "scanned an empty str at line %zu, column %zu. this is not "
              "right.\n",
              line, col);
          break;
        }
        // take out the substr
        char *subbed = myalloc(pioneer - ptr + 1);
        assert(subbed);
        memcpy(subbed, &str[ptr], pioneer - ptr);
        subbed[pioneer - ptr] = '\0';
        // final check and add it to the tokens
        if (!scan_recipe[i].after(subbed, &tokens, (filepos_t){line, col})) {
          // failed final check, freeing the str.
          LOG(VERBOSE, "failed final check\n");
          myfree(subbed);
        } else {
          LOG(VERBOSE, "taken token %s\n", subbed);
          // succeeded.
          token_flag = true;
          forward(str, &ptr, pioneer - ptr, &line, &col);
          break;
        }
      }
    }
    if (!token_flag) {
      cry_errorf(SENDER_LEXER, ((filepos_t){.line = line, .column = col}),
                 "met an token-untranslatable str\n");
      break;
    }
  }
  return tokens;
}

token_t create_token(tokentype_t token_type, char *value, filepos_t pos) {
  token_t tok={0};
  tok.token_type = token_type;
  tok.value = value;
  tok.position = pos;
  return tok;
}
static char *mappings[] = {
    [LET] = "LET",
    [FN] = "FN",
    [IF] = "IF",
    [ELSE] = "ELSE",
    [WHILE] = "WHILE",
    [RETURN] = "RETURN",
    [EXTERN] = "EXTERN",
    [BREAK] = "BREAK",
    [INT] = "INT",
    [STRING] = "STRING",
    [WHITESPACE] = "WHITESPACE",
    [COMMA] = "COMMA",
    [SEMICOLON] = "SEMICOLON",
    [COLON] = "COLON",
    [QUOTE] = "QUOTE",
    [DOUBLE_QUOTE] = "DOUBLE_QUOTE",
    [OPENPAREN] = "OPENPAREN",
    [CLOSEPAREN] = "CLOSEPAREN",
    [OPENBRACE] = "OPENBRACE",
    [CLOSEBRACE] = "CLOSEBRACE",
    [ADD] = "ADD",
    [SUB] = "SUB",
    [MUL] = "MUL",
    [DIV] = "DIV",
    [MOD] = "MOD",
    [BITAND] = "BITAND",
    [BITOR] = "BITOR",
    [XOR] = "XOR",
    [EQUAL] = "EQUAL",
    [GREATER] = "GREATER",
    [LESS] = "LESS",
    [GREATER_EQUAL] = "GREATER_EQUAL",
    [LESS_EQUAL] = "LESS_EQUAL",
    [NOT_EQUAL] = "NOT_EQUAL",
    [ASSIGN] = "ASSIGN",
    [AND] = "AND",
    [OR] = "OR",
    [NOT] = "NOT",
    [IDENTIFIER] = "IDENTIFIER",
    [CONSTANT_NUMBER] = "CONSTANT_NUMBER",
    [CONSTANT_STRING] = "CONSTANT_STRING",
    [CONSTANT_CHAR] = "CONSTANT_CHAR",
    [TOKEN_VALUE] = "TOKEN_VALUE",
    [TOKEN_ID] = "TOKEN_ID",
    [TOKEN_TYPEKW] = "TOKEN_TYPEKW",
    [TOKEN_EXPR] = "TOKEN_EXPR",
    [TOKEN_STATEMENTS] = "TOKEN_STATEMENTS",
    [TOKEN_ARGLIST] = "TOKEN_ARGLIST",
};
char *tokentype_tostr(tokentype_t tt) { return mappings[tt]; }
void free_token(token_t *tok) { FREEIFD(tok->value, myfree); }
