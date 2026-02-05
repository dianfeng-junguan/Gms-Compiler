#include "sematic.h"
#include "parser.h"
#include "lexer.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************/
/* the checker should decide whether to check the subnodes */
/***********************************************************/

typedef struct {
  // name of this type  
  char *name;
  // memsize it takes up. class will add up the sizes of the members but the
  // final size depends on the memory alignment chosen.  
  size_t size;
  // if this is a class, it records the members  
  list_t members;
} class_type_t;
int get_type_from_typekwnode(astnode_t *typekwnode);
size_t while_depth = 0;
int function_rettype = -1;
bool in_function = false;
list_t *current_func_arglist = NULL;
list_t type_table = {0};
static symbol_type_t intrinsic_types[] = {
    {.name = "void", .size = 0},
    {.name = "int", .size = 8},
    {.name = "string", .size = 8},// string is actually just a pointer
    
};
bool check_node(astnode_t* node,list_t* symbols, int layer);
bool check_arglist(astnode_t* commalist, list_t* symbols, list_t* arglist, size_t index, filepos_t pos, int layer);

/**
   reminder: name must lives longer than the symbol!!!
 **/
symbol_t create_symbol(char* name, symbol_kind_t type, int value_type, int layer){
  return (symbol_t){
    .name=clone_str(name),
    .type=type,
    .sym_type=value_type,
    .layer=layer,
    .value=0,  
    .is_extern=false
  };
}
void copy_symbol(symbol_t* old, symbol_t* newt){
  newt->name=clone_str(old->name);
}
void free_symbol(symbol_t* sym){
  FREEIFD(sym->name, myfree);
  sym->name=NULL;
  FREEIFD(sym, myfree);
}

bool sym_redef_trigger(astnode_t *node) {
  switch (node->node_type) {
  case NODE_DEFINITION: 
  case NODE_DECLARE_FUNC:
  case NODE_DECLARE_VAR:
  case NODE_FUNCTION:
  case NODE_ARGPAIR:
    return true;
  default:
    return false;
  }
}
bool sym_redef_checker(astnode_t *node, list_t *symbols, int layer) {
  // NODE_DEFINITION  NODE_DECLARE_FUNC  NODE_DECLARE_VAR  NODE_FUNCTION
  // the nodes above all store the identifier at their left subnode
  char *name = node->left->value;
  symbol_type_index_t vartype=0;
  switch (node->node_type) {
  case NODE_DECLARE_FUNC:
    name = NODE_FUNCDECL_ID(node)->value;
    vartype=get_type_from_typekwnode(NODE_FUNCDECL_TYPEKW(node));    
    break;    
  case NODE_DECLARE_VAR:
    name = NODE_VARDECL_ID(node)->value;
    vartype = get_type_from_typekwnode(NODE_VARDECL_TYPEKW(node));    
    break;    
  case NODE_FUNCTION:
    name = NODE_FUNC_ID(node)->value;
    // get the return type
    vartype=get_type_from_typekwnode(NODE_FUNC_TYPEKW(node));
    break;    
  case NODE_DEFINITION:
    name = NODE_DEF_ID(node)->value;
    vartype = NODE_DEF_TYPEKW(node)
                  ? get_type_from_typekwnode(NODE_DEF_TYPEKW(node))
                  : node->right->value_type;
    // check the types of rexpr and type annotation
    if(node->right->value_type!=vartype){
      cry_errorf(SENDER_SEMATIC, node->position,
                 "type annotation does not conform to the right expression.\n");
      return false;
    }
    break;
  case NODE_ARGPAIR:
    name = NODE_ARGPAIR_ID(node)->value;
    vartype = get_type_from_typekwnode(NODE_ARGPAIR_TYPEKW(node));    
    break;    
  default:panic("redef met uncopable node type")break;
  }  
  if(is_symtab_dup(&node->syms, name)){
    cry_errorf(SENDER_SEMATIC, node->position, "variable redefined or redeclared:%s",
               node->left->value);
    return false;
  }
  if(node->node_type==NODE_FUNCTION){
    // for NODE_FUNCTION, we need to init the symbol table
    init_list(&node->syms, symbols->capacity, sizeof(symbol_t));
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
  }else if(node->node_type==NODE_DEFINITION){
    if(!check_node(node->right,symbols, layer)){
      return false;
    }
  }
  // ok. add it to the symtab.
  symbol_t defedsym =
      create_symbol(name, SYMBOL_VARIABLE, vartype, layer);
  LOG(VERBOSE,"added symbol %s\n",name);
  defedsym.is_extern=true;
  // for func declaration we need to scan the arglist  
  if(node->node_type==NODE_DECLARE_FUNC||node->node_type==NODE_FUNCTION){
    init_list(&defedsym.args, 6, sizeof(symbol_type_index_t));
    defedsym.type=SYMBOL_FUNCTION;
    if(node->node_type==NODE_FUNCTION){
      node->value_type = node->left->right->value_type;
    }
    current_func_arglist = &defedsym.args;
    in_function=true;
    function_rettype=node->value_type;
    // use the node's symbol tab to prevent the arg variable from leaking into
    // outer scope
    // check the arglist
    if (!check_node(node->node_type == NODE_DECLARE_FUNC ? node->right
                                                         : node->right->left,
                    &node->syms, layer)) {
      /*func node:
        -holder       -holder
         -id -rettype  -args -body
        func decl node:
         -holder        -args
	  -id -rettype
      */
      return false;
    }
    current_func_arglist = NULL;
    function_rettype=-1;
    in_function=false;
  }
  if (node->node_type == NODE_ARGPAIR) {
    symbol_t sym=create_symbol(node->left->value, SYMBOL_VARIABLE, node->right->value_type, layer);
    // add it to the function symbol arglist
    // argtype is small enough so we use list_t element to store it directly (though it's void*)
    if(!current_func_arglist){
      cry_error(SENDER_SEMATIC, "function arglist not in a function",
                node->position);
      return false;
    }
    append(current_func_arglist, &node->right->value_type);
    // ok. add it to the symtab.
    list_append(symbols, &sym);
  } else {
    list_append(symbols, &defedsym);
  }
  // for function and definition we need to check the body or right expr
  if (node->node_type == NODE_FUNCTION) {
    in_function=true;
    function_rettype=node->value_type;
    bool suc = check_node(node->right->right, symbols, layer);
    in_function = false;
    function_rettype=0;
    if(!suc)return false;
  }
  return true;
}

bool typereg_trigger(astnode_t *node) {
  switch (node->node_type) {
  case NODE_DEFINITION: 
  case NODE_DECLARE_FUNC:
  case NODE_DECLARE_VAR:
  case NODE_FUNCTION:
  case NODE_ARGPAIR:
    return true;
  default:
    return false;
  }
}
int find_refer_type_of(symbol_type_index_t ind) {
  symbol_type_t *stype = list_get(&type_table, ind);
  if (!stype)
    return -1;
  int ptrl = stype->pointer_level;
  int ptrto= ind;
  while(stype->pointer_level>0){
    // switch to the inital non-pointer type
    ptrto = stype->point_to;    
    stype = list_get(&type_table, stype->point_to);
    ptrl++;
  }
  for (int i = 0; i < sizeof(type_table); ++i) {
    symbol_type_t* iter=list_get(&type_table, i);
    if (iter->point_to==ptrto&&iter->pointer_level==ptrl+1) {
      return i;
    }
  }
  // no existing type. create new one
  char *newtypename = malloc(strlen(stype->name + ptrl + 2));
  strcpy(newtypename, stype->name);
  size_t rawtype_namelen=strlen(stype->name);
  for (int i=0; i < ptrl+1; ++i) {
    newtypename[rawtype_namelen+i]='*';
  }
  newtypename[rawtype_namelen+ptrl]='\0';
  symbol_type_t newptrtype = {
      .name = newtypename, .point_to = ptrto, .pointer_level = ptrl+1};
  append(&type_table, &newptrtype);
  return type_table.element_size;
}
int find_defer_type_of(symbol_type_index_t ind) {  
  symbol_type_t *stype = list_get(&type_table, ind);
  if (!stype)
    return -1;
  int ptrl = stype->pointer_level;
  if(ptrl==0){
    // cannot defer anymore
    return -1;
  }
  int ptrto= ind;
  while(stype->pointer_level>0){
    // switch to the inital non-pointer type
    ptrto = stype->point_to;    
    stype = list_get(&type_table, stype->point_to);
    ptrl++;
  }
  for (int i = 0; i < sizeof(type_table); ++i) {
    symbol_type_t* iter=list_get(&type_table, i);
    if (iter->point_to==ptrto&&iter->pointer_level==ptrl-1) {
      return i;
    }
  }
  // no existing type. create new one
  char *newtypename = malloc(strlen(stype->name + ptrl + 1));
  strcpy(newtypename, stype->name);
  size_t rawtype_namelen=strlen(stype->name);
  for (int i=0; i < ptrl-1; ++i) {
    newtypename[rawtype_namelen+i]='*';
  }
  newtypename[rawtype_namelen+ptrl-1]='\0';
  symbol_type_t newptrtype = {
      .name = newtypename, .point_to = ptrto, .pointer_level = ptrl-1};
  append(&type_table, &newptrtype);
  return type_table.element_size;
}
bool is_pointer_type(symbol_type_index_t ind) {
  symbol_type_t* iter=list_get(&type_table, ind);
  assert(iter);
  return iter->pointer_level>0;
}
int get_type_from_typekwnode(astnode_t *typekwnode) {
  char *typestr = typekwnode->value;
  if (!typestr) {
    panic("typekw node has NO VALUE STR");
  }
  // we need to check if it is a pointer type that has not been added
  char *ptr = typestr;
  while (*(ptr++)!='*') {
  }
  char *proto = malloc(ptr - typestr + 1);
  memcpy(proto, typestr, ptr - typestr);
  int ptrlvl = 0;
  while (*(ptr++) == '*') {
    ptrlvl++;
  }
  for (int i = 0; i < type_table.len; ++i) {
    symbol_type_t *symtype=list_get(&type_table,i);
    if (strcmp(proto, symtype->name) == 0) {
      if (ptrlvl == 0) {
	//exactly the type 
	return i;
      }else{
        symbol_type_t ptrtype = {.name = clone_str(typestr),
                                 .pointer_level = ptrlvl};
        append(&type_table, &ptrtype);
	return type_table.len-1;
      }
    }
  }
  panic("type or type pointed to not found ");
  return -1;
}

bool kwtype_trigger(astnode_t *node) {
  return node->node_type==NODE_TYPEKW;
}
bool kwtype_checker(astnode_t *node, list_t *symbols, int layer) {
    int argtype = get_type_from_typekwnode(node);
    node->value_type = argtype;
    return true;
}

bool ctypeinf_trigger(astnode_t *node) {
  return node->node_type==NODE_CONSTANT;
}
bool ctypeinf_checker(astnode_t *node, list_t *symbols, int layer) {
    switch (node->extra_info) {
    case CONSTANT_CHAR: 
    case CONSTANT_NUMBER: {
      for (int i=0; i < type_table.len; ++i) {
        symbol_type_t *stype = list_get(&type_table, i);
        if (strcmp(stype->name,"int")==0) {
          node->value_type = i;
	  break;
	}
      }
      break;
    }
    case CONSTANT_STRING:
      for (int i=0; i < type_table.len; ++i) {
        symbol_type_t *stype = list_get(&type_table, i);
        if (strcmp(stype->name,"string")==0) {
          node->value_type = i;
	  break;
	}
      }
      LOG(VERBOSE, "sematic: constant_string set\n");      
      break;
    default:
      cry_errorf(SENDER_SEMATIC, node->position, "met unsupported constant type extra info:%zu\n",node->extra_info);
      break;
    }
    return true;
}
/// used to analyze the type of the defined symbols
bool typereg_checker(astnode_t *node, list_t *symbols, int layer) {
  
  return true;  
}
bool sym_undef_trigger(astnode_t *node) {
  return node->node_type==NODE_IDENTIFIER;
}
bool sym_undef_checker(astnode_t *node, list_t *symbols, int layer) {
  for (size_t i=0; i < symbols->len; ++i) {
    symbol_t* sym=list_get(symbols, i);
    assert(node->value);
    if(strcmp(node->value, sym->name)==0){
      // found
      LOG(VERBOSE, "identifier found defined symbol %s, type %d\n",
          node->value, sym->sym_type);
      // update the value type of the node      
      node->value_type=sym->sym_type;
      return true;
    }
  }
  cry_errorf(SENDER_SEMATIC, node->position, "undefined variable:%s\n", node->value);
  return false;
}
bool incontype_trigger(astnode_t *node) {
  switch (node->node_type) {
  case NODE_ADD:case NODE_SUB: 
  case NODE_MUL:case NODE_DIV:
  case NODE_MOD:case NODE_AND:
  case NODE_OR:case NODE_BITAND:
  case NODE_BITOR:case NODE_XOR:
  case NODE_ASSIGN:case NODE_EQUAL:
  case NODE_GREATER:case NODE_LESS:
  case NODE_GREATER_EQUAL:
  case NODE_LESS_EQUAL:
  case NODE_COMMALIST:
    return true;
  default:
    return false;
    break;
  }
}
bool incontype_checker(astnode_t *node, list_t *symbols, int layer) {
  // infer the type of the expression
  if (!check_node(node->left, symbols, layer) ||
      !check_node(node->right, symbols, layer)) {
    return false;
  }
  if(symtypcmp(node->left->value_type,node->right->value_type)!=0){
    cry_error(SENDER_SEMATIC, "left expression type and right expression type are not the same", node->position);
    return false;
  }
  // infer
  node->value_type = node->left->value_type;
  return true;
}
bool funccall_arg_trigger(astnode_t *node){
  return node->node_type==NODE_FUNCCALL;
}
bool funccall_arg_checker(astnode_t *node, list_t *symbols, int layer) {
  // check left node first
  if(!check_node(node->left, symbols, layer)){
    return false;
  }
  // check args passed on the right  
  for (size_t i=0; i < symbols->len; ++i) {
    symbol_t *sym=list_get(symbols, i);
    if(strcmp(node->left->value, sym->name)==0){
      // infer the type
      node->value_type=sym->return_type;
      // check the args
      if (!check_arglist(node->right, symbols, &sym->args, 0,
                         node->position, layer)) {
	cry_errorf(SENDER_SEMATIC, node->position, "wrong argument type");
	return false;
      }
    }
  }
  return true;
}
symbol_t *find_symbol(char* name, list_t* symbols){
  for (size_t i=0; i < symbols->len; ++i) {
    symbol_t *sym=list_get(symbols, i);
    if (strcmp(name, sym->name) == 0) {
      return sym;
    }
  }
  return NULL;
}
bool exismem_trigger(astnode_t *node) {
  return node->node_type==NODE_PROPERTY;
}
bool exismem_checker(astnode_t *node, list_t *symbols, int layer) {
  symbol_t *sym = find_symbol(node->left->value, symbols);
  if (!sym) {
    cry_errorf(SENDER_SEMATIC, node->position, "undefined symbol %s\n",node->left->value);
    return false;
  }
  symbol_type_t *stype = list_get(&type_table, sym->sym_type);
  // look for member
  char *memname = node->right->value;
  for (size_t i=0; i < stype->members.len; ++i) {
    name_type_pair_t *p = list_get(&stype->members, i);
    if (strcmp(memname, p->name) == 0) {
      // infer type
      node->value_type=p->type;
      return true;
    }
  }
  cry_errorf(SENDER_SEMATIC, node->position, "member %s not found in %s\n",
             memname, sym->name);
  return false;
}
bool return_trigger(astnode_t *node) {
  return node->node_type==NODE_RETURN;
}
bool return_checker(astnode_t *node, list_t *symbols, int layer) {
  // check if in function
  if(!in_function){
    cry_error(SENDER_SEMATIC, "return not in function", node->position);
    return false;
  }  
  // check right expr
  int rett=-1;
  if(node->left){
    if(check_node(node->left, symbols, layer))
      rett = node->left->value_type;
    else
      return false;
  }
  // check whether the return type meets the function type
  if(symtypcmp(function_rettype,rett)<0){
    cry_errorf(SENDER_SEMATIC, node->position, "return type does not meet the function type");
    return false;
  }
  return true;
}
bool refer_trigger(astnode_t *node) {
  return node->node_type==NODE_REFER;
}
bool refer_checker(astnode_t *node, list_t *symbols, int layer) {
  if (!node->right || !check_node(node->right, symbols, layer)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    // we only allow referring a variable
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }
  // infer the type of the expression
  node->value_type=find_refer_type_of(node->right->value_type);
  return true;  
}

bool defer_trigger(astnode_t *node){return node->node_type==NODE_DEFER;}
bool defer_checker(astnode_t *node,list_t* symbols, int layer) {  
  if (!node->right || !check_node(node->right, symbols, layer)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    // we only allow referring a variable
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }  
  if(!is_pointer_type(node->right->value_type)){
    cry_error(SENDER_SEMATIC, "trying to defer a non-pointer value or variable",
              node->position);
    return false;
  }
  // infer the type of the expression
  node->value_type=find_defer_type_of(node->right->value_type);
  return true;  
}
bool break_trigger(astnode_t *node){return node->node_type==NODE_RETURN;}
bool break_checker(astnode_t *node, list_t *symbols, int layer) {
  if(while_depth==0){
    cry_error(SENDER_SEMATIC, "found break not in while", node->position);
    return false;
  }
  return true;;  
}

bool condition_block_trigger(astnode_t *node){
  switch (node->node_type) {
  case NODE_IF: 
  case NODE_ELSEIF: 
  case NODE_ELSE: 
  case NODE_WHILE: 
    return true;
  default:
    return false;
  }
}
bool condition_block_checker(astnode_t *node, list_t *symbols, int layer) {
  // the body of the if is a scope
  init_list(&node->syms, 10, sizeof(symbol_t));
  list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
  // check body
  if(node->left)
    check_node(node->left, &node->syms, layer);
  else{
    cry_error(SENDER_SEMATIC, "no statment body", node->position);
    return false;
  }
  return true;  
}

bool otherschk_trigger(astnode_t *node) {
  return node->node_type==NODE_LEAFHOLDER;
}
bool otherschk_checker(astnode_t* node ,list_t *symbols, int layer) {
  return (node->left?check_node(node->left, symbols, layer):true)&&
    (node->right?check_node(node->right, symbols, layer):true);
}
bool objdef_typeinf_trigger(astnode_t *node) {
  return node->node_type==NODE_CLASSFILL;
}
bool objdef_typeinf_checker(astnode_t *node, list_t *symbols, int layer){
  /*
    infer the type of the classfill node so that the type system works
    correctly
   */
#define NODE_CLASSFILL_CLASSNAME(node) (node->left)
  char *classname = NODE_CLASSFILL_CLASSNAME(node)->value;
  for (size_t i=0; i < type_table.len; ++i) {
    symbol_type_t *stype = list_get(&type_table, i);
    if (strcmp(stype->name, classname) == 0) {
      node->value_type = i;
      return true;     
    }
  }
  cry_errorf(SENDER_SEMATIC, node->position, "class not found:%s\n",classname);
  return false;
}
bool class_def_trigger(astnode_t *node) {
  return node->node_type == NODE_CLASS;
}

void init_sematic(){
  init_list(&type_table, 10, sizeof(symbol_type_t));
  // add intrinsic types
  for (int i = 0; i < sizeof(intrinsic_types) / sizeof(symbol_type_t); i++) {
    append(&type_table, &intrinsic_types[i]);
  }
}
/* TODO: intrinsic types in type table */
bool class_def_checker(astnode_t *node, list_t *symbols, int layer) {
  // add the class name to the class type name table
  symbol_type_t class_type = {
    .name = clone_str(node->left->value),
    .size=0,
    .members = create_list(10, sizeof(name_type_pair_t))
  };  
  list_t tovisit = create_list(32, sizeof(astnode_t *));
  append(&tovisit, &node->right);
  size_t i = 0;
  while (i < tovisit.len) {
    astnode_t *subnode = *(astnode_t **)list_get(&tovisit, i);
    if (!subnode) {
      i++;
      continue;
    }
    if (subnode->node_type==NODE_CLASSMEMBER){
      // subnodes are name and type
      astnode_t *idnode = subnode->left;
      astnode_t *typenode = subnode->right;
      name_type_pair_t member = {.name = clone_str(idnode->value),
				 .type = get_type_from_typekwnode(typenode)};
      append(&class_type.members, &member);
    }else if(subnode->node_type==NODE_LEAFHOLDER){
      // add to tovisit
      if(subnode->left)append(&tovisit, &subnode->left);
      if(subnode->right)append(&tovisit, &subnode->right);
    }    
    i++;
  }
  append(&type_table, &class_type);
  LOG(VERBOSE, "class type registered:%s\n",class_type.name);
  free_list(&tovisit);
  return true;
}
// things need to be done before checking
static rule_t sematic_preprocess_list[] = {
    {.name = "keyword type infer",
     .trigger = kwtype_trigger,
     .checker = kwtype_checker},
    {.name = "constant type infer",
     .trigger = ctypeinf_trigger,
     ctypeinf_checker},
    {.name = "class definition",
     .trigger = class_def_trigger,
     .checker = class_def_checker},
    {.name = "object definition type infer",
     .trigger = objdef_typeinf_trigger,
     .checker = objdef_typeinf_checker},    
};

static rule_t sematic_rules[] = {
  {.name = "symbol redefinition",
   .trigger = sym_redef_trigger,
   .checker = sym_redef_checker},
  {.name = "type register",
   .trigger = typereg_trigger,
     .checker = typereg_checker},
  {.name="undefined symbol",.trigger=sym_undef_trigger,.checker=sym_undef_checker},
  {.name="consistent left and right value type",.trigger=incontype_trigger,.checker=incontype_checker},
  {.name="funccall argcheck",.trigger=funccall_arg_trigger,.checker=funccall_arg_checker},
  {.name="existent member",.trigger=exismem_trigger,.checker=exismem_checker},
  {.name="return check",.trigger=return_trigger,.checker=return_checker},
  {.name="refer check",.trigger=refer_trigger,.checker=refer_checker},
  {.name="defer check",.trigger=defer_trigger,.checker=defer_checker},
  {.name="break in while",.trigger=break_trigger,.checker=break_checker},
  {.name="scope symbol copier",.trigger=condition_block_trigger,.checker=condition_block_checker},
  {.name="others",.trigger=otherschk_trigger,.checker=otherschk_checker},
};
bool check_node(astnode_t *node, list_t *symbols, int layer) {
  for (size_t i=0; i < sizeof(sematic_rules)/sizeof(rule_t); ++i) {
    rule_t* r=&sematic_rules[i];
    if (r->trigger(node)) {
      LOG(VERBOSE, "checking node %s with %s\n", get_nodetype_str(node->node_type), r->name);
      if(!r->checker(node,symbols, layer)){
	return false;
      }
      break;      
    }
  }
  return true;  
}
bool is_symtab_dup(list_t* syms, char* name){
  for (size_t i=0; i < syms->len; ++i) {
    symbol_t* sym=list_get(syms, i);
    if(strcmp(sym->name, name)==0){
      return true;
    }
  }
  return false;
}
/*
  check the type of passed arguments.
 */
bool check_arglist(astnode_t* commalist, list_t* symbols, list_t* arglist, size_t index, filepos_t pos, int layer){
  if(commalist->node_type==NODE_COMMALIST){
    // commalist. check subnodes
    if(commalist->left){
      if(!check_arglist(commalist->left, symbols, arglist, index, pos, layer))return false;
    }
    if(commalist->right){
      if(!check_arglist(commalist->right, symbols, arglist, index+1, pos, layer))return false;
    }
    return true;
  }
  if(arglist->len<=index){
    cry_error(SENDER_SEMATIC, "too many arguments", pos);
    return false;
  }
  // an expr, check its value type
  if(!check_node(commalist, symbols, layer)){
    return false;
  }
  symbol_type_index_t passedtype=commalist->value_type;
  symbol_type_index_t argtype=*(symbol_type_index_t*)list_get(arglist, index);
  if(symtypcmp(passedtype,argtype)!=0){
    cry_errorf(SENDER_SEMATIC, pos, "expected type %d , found %d\n",
	       (argtype),(passedtype));
    return false;
  }
  return true;
}
bool preprocess_tree(astnode_t* ast){
  bool suc = true;
  // preprocess the ast from the bottom because we want to
  // infer types from the leaves  
  if(ast->left)suc=preprocess_tree(ast->left)?suc:false;
  if(ast->right)suc=preprocess_tree(ast->right)?suc:false;
  for (int i = 0; i < sizeof(sematic_preprocess_list) / sizeof(rule_t); ++i) {

    if (sematic_preprocess_list[i].trigger(ast)) {
      // there is no symbol table nor layer when preprocessing
      if(!sematic_preprocess_list[i].checker(ast,NULL,0)){
	suc=false;// do not stop right away
      }
    }
  }
  return suc;
}
bool do_sematic(astnode_t* ast){
  // init the symbol table
  init_list(&ast->syms, 10, sizeof(symbol_t));
  // preprocess
  if(!preprocess_tree(ast)){
    cry_error(SENDER_SEMATIC, "failed preprocessing", ast->position);
  }
  return check_node(ast, &ast->syms,0);
}
