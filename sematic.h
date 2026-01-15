#include <stdbool.h>
#include "utils.h"
typedef struct _astnode_t astnode_t;
/*
 * do sematic check. prints out error and warning.
 */
bool do_sematic(astnode_t* ast);
bool is_symtab_dup(list_t* syms, char* name);
