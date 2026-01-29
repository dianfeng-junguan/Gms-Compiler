#include <stdbool.h>
#include "utils.h"
typedef struct _astnode_t astnode_t;
typedef bool (*sematic_trigger_t)(astnode_t*);
typedef bool (*sematic_checker_t)(astnode_t*,list_t*,int);
typedef struct{
  char *name;
  // check if the checker should be called on the node
  sematic_trigger_t trigger;
  // check the node  
  sematic_checker_t checker;
}rule_t;
/*
 * do sematic check. prints out error and warning.
 */
bool do_sematic(astnode_t* ast);
bool is_symtab_dup(list_t* syms, char* name); 

