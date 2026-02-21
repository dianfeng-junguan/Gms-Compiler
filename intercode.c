#include "intercode.h"
#include "err.h"
#include "parser.h"
#include "sematic.h"
#include "stdbool.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**


 **/
static int labelid = 0;
char *mklabel() {
  int id = labelid;
  int digits = (id == 0 ? 1 : 0);
  while (id > 0) {
    id /= 10;
    digits++;
  }
  char *number = malloc(digits + 1);
  sprintf(number, "%d", labelid);
  char *lbl = malloc(5 + digits + 1);
  strcpy(lbl, "label");
  lbl = strcat(lbl, number);
  labelid++;
  return lbl;
}
void allocate_syms(list_t *code_list, symbol_table_t *syms) {
  for (size_t i = 0; i < syms->table.len; ++i) {
    symbol_t *sym = list_get(&syms->table, i);
    
    symbol_type_t* stype= list_get(&type_table,sym->sym_type);
    size_t typesize = stype->size;
    do_log(VERBOSE, INTERCODE_ALLOCSYM, "%s for sym %s@%d type %d size %zu\n",__FUNCTION__, sym->name, sym->layer, sym->sym_type, stype->size);    
    if (sym->is_extern||sym->type!=SYMBOL_VARIABLE) { 
      continue;
    }
    push_code(code_list, CODE_ALLOC_LOCAL, KEEP(sym->name), imm_num(typesize),
              EMPTY);
  }    
}
/// push a intercode into the list
void push_code(list_t *code_list, intercode_type_t code_type, operand_t op1,
               operand_t op2, operand_t op3) {
  intercode_t code = create_code(code_type, op1, op2, op3);
  append(code_list, &code);
}
static int TMPVAR_ALLOC = 0;
tmpvar_t make_tmpvar() {
  tmpvar_t tmpv = {.index = TMPVAR_ALLOC++};
  return tmpv;
}
/// a tool func to create a code on the heap by a local intercode.
intercode_t create_code(intercode_type_t type, operand_t operand1,
                        operand_t operand2, operand_t operand3) {
  return (intercode_t){
    .type = type,
    .op1 = operand1,
    .op2 = operand2,
    .op3 = operand3,
  };
}
void free_intercode(intercode_t *code) {
  /* TODO: judge the operand type and free*/
}
tmpvar_t gen_node(astnode_t *node, list_t *code_list, int tmpnum, int layer);
size_t gen_arglist_node(astnode_t *node, list_t *code_list, int tmpnum,
                        int layer, list_t *tmpvars) {
  size_t num_args = 0;
  // used to gen intercode for commalist in NODE_FUNCCALL
  switch (node->node_type) {
  case NODE_COMMALIST: {
    // more args
    num_args += gen_arglist_node(node->left, code_list, tmpnum, layer, tmpvars);
    num_args +=
      gen_arglist_node(node->right, code_list, tmpnum, layer, tmpvars);
    break;
  }
  default: {
    // an arg
    tmpvar_t arg = gen_node(node, code_list, tmpnum, layer);
    append(tmpvars, &arg);
    // push the arg
    push_code(code_list, CODE_PUSHARG, TMP(arg), EMPTY, EMPTY);
    // one arg
    num_args = 1;
    break;
  }
  }
  return num_args;
}
/* this is used for breaks */
static char *while_done_lbls[32];
static size_t while_done_ptr = 0;
void push_while_done_label(char *done_label) {
  while_done_lbls[while_done_ptr++] = done_label;
  assert(while_done_ptr < 32);
}
void pop_while_done_label() {
  while_done_ptr--;
  assert(while_done_ptr >= 0);
}
char *get_top_while_done_label() {
  if (while_done_ptr == 0)
    return NULL;
  return while_done_lbls[while_done_ptr - 1];
}
/**
 * get the offset of the member in the class.
 **/
size_t get_member_offset(symbol_type_index_t object_type, char *membername) {
  // now get the offset of this member in the class
  symbol_type_t* stype=list_get(&type_table, object_type);
  list_t* members=&stype->members;
  size_t offset=0;
  for (size_t j=0; j<members->len; j++) {
    name_type_pair_t *pair = list_get(members, j);
    if (strcmp(pair->name,membername)==0) {
      // found the member          
      break;
    }
    symbol_type_t *member_type=list_get(&type_table, pair->type);
    offset+=member_type->size;
  }
  return offset;  
}
/// generate intercodes of classfill
void gen_classfill(list_t *code_list, astnode_t *node, tmpvar_t *tmpvar) {
  if(!node)return;
  // node is a NODE_CLASSFILL
  // get the type of the class
  symbol_type_t *classtype = list_get(&type_table, node->value_type);
  list_t *members=&classtype->members;
  assert(classtype);
  list_t tovisit = create_list(20, sizeof(astnode_t *));
  append(&tovisit, &node->left);
  append(&tovisit, &node->right);
  int i = 0;
  while (i<tovisit.len) {
    astnode_t *visit = *(astnode_t**)list_get(&tovisit, i);
    switch (visit->node_type) {
    case NODE_COMMALIST:
    case NODE_CLASSFILL:
    case NODE_LEAFHOLDER: {
      append(&tovisit, &visit->left);
      append(&tovisit, &visit->right);
      break;
    }
    case NODE_KEYVALUE_PAIR: {
      // key:value
      char* key= visit->left->value;
      char *value = visit->right->value;
      // now get the offset of this member in the class
      size_t offset=0;
      for (size_t j=0; j<members->len; j++) {
        name_type_pair_t *pair = list_get(members, j);
	if (strcmp(pair->name,key)==0) {
          // found the member
          push_code(code_list, CODE_MOV, OFFSETTMP(*tmpvar, offset),
                    VALUE(value), EMPTY);          
	  break;
        }	symbol_type_t *member_type=list_get(&type_table, pair->type);
	offset+=member_type->size;
      }
      break;
    }      
    default:
      cry_errorf(SENDER_INTERCODER, node->position, "other node in classfill: %s?\n",get_nodetype_str(node->node_type));
      break;    
    }    
    i++;
  }
  free_list(&tovisit);
}
/// generate intercode of a node.
/// returns the struct describing the tmp var which stores the value of the
/// expression if
/// the translated node is an expression
/// format: tmp123
/// args:
/// tmpnum: the num of the tmp vars already in existence
tmpvar_t gen_node(astnode_t *node, list_t *code_list, int tmpnum, int layer) {
  // reminder: each node should generate at most one temp var.
  switch (node->node_type) {
  case NODE_FUNCTION: {
    assert(node->left && (node->left->left->node_type == NODE_IDENTIFIER) &&
           node->left->left->value);
    char *funcname = node->left->left->value;
    // first declare the func
    push_code(code_list, CODE_DEF_FUNC, KEEP(funcname), EMPTY, EMPTY);
    // then assign the symbols
    allocate_syms(code_list, node->syms);
    // then gen the body
    gen_node(node->right, code_list, tmpnum, layer + 1);
    push_code(code_list, CODE_DEF_FUNC_END, KEEP(funcname), EMPTY, EMPTY);
    break;
  }
  case NODE_RETURN: {
    tmpvar_t tmpv;
    if (node->left) {
      // gen the return value first
      tmpv = gen_node(node->left, code_list, tmpnum, layer + 1);
    }
    push_code(code_list, CODE_RETURN, TMP(tmpv), EMPTY, EMPTY);
    
    break;
  }
  case NODE_ASSIGN: {
    assert(node->left && node->left->value && node->right);
    char *assigned = node->left->value;
    tmpvar_t rhs = gen_node(node->right, code_list, tmpnum, layer + 1);
    push_code(code_list, CODE_MOV, VALUE(assigned), TMP(rhs), EMPTY);
    
    break;
  }
  case NODE_DEFINITION: {
    tmpvar_t rhs = gen_node(node->right, code_list, tmpnum, layer + 1);
    push_code(code_list, CODE_MOV, VALUE(node->left->left->value), TMP(rhs), EMPTY);
    
    break;
  }
#define TWOOP_INTERCODE(type_noprefix)					\
    case NODE_##type_noprefix: {					\
      assert(node->left && node->right);				\
      tmpvar_t lhs = gen_node(node->left, code_list, tmpnum, layer + 1); \
      tmpvar_t rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 2); \
      tmpvar_t res = make_tmpvar();					\
      push_code(code_list, CODE_ALLOC_TMP, TMP(res), IMM("8"), EMPTY);	\
      push_code(code_list, CODE_##type_noprefix, TMP(lhs), TMP(rhs), TMP(res));	\
									\
									\
      return res;							\
      break;								\
    }

  case NODE_COMMALIST: {
    // return the value of the rightest part
    gen_node(node->left, code_list, tmpnum, layer + 1);
    tmpvar_t rexpr = gen_node(node->right, code_list, tmpnum, layer + 1);
    return rexpr;
    break;
  }

    TWOOP_INTERCODE(ADD);
    TWOOP_INTERCODE(SUB);
    TWOOP_INTERCODE(MUL);
    TWOOP_INTERCODE(DIV);
    TWOOP_INTERCODE(MOD);

    TWOOP_INTERCODE(BITAND);
    TWOOP_INTERCODE(BITOR);

  case NODE_REFER: {
    tmpvar_t res = make_tmpvar();
    push_code(code_list, CODE_REFER, TMP(res), ADDR(node->right->value), EMPTY);
    return res;
    break;
  }
  case NODE_DEFER: {
    tmpvar_t res = make_tmpvar();
    push_code(code_list, CODE_DEFER, TMP(res), ADDR(node->right->value), EMPTY);
    return res;
    break;
  }
    // TWOOP_INTERCODE(BITNOT)
    /*
      the comparing node returns 1 if true, otherwise 0 as false.
    */
#define CMP_INTERCODE(type_noprefix, jmpcode)                                  \
    case NODE_##type_noprefix: {					\
      assert(node->left && node->right);				\
      tmpvar_t lhs = gen_node(node->left, code_list, tmpnum, layer + 1); \
      tmpvar_t rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 1); \
      tmpvar_t tmpv = make_tmpvar();					\
      push_code(code_list, CODE_CMP, TMP(lhs), TMP(rhs), EMPTY);	\
      push_code(code_list, CODE_ALLOC_TMP, TMP(tmpv), IMM("8"), EMPTY);	\
      char *donelbl = mklabel();					\
      char *truelbl = mklabel();					\
      push_code(code_list, CODE_##jmpcode, ADDR(truelbl), EMPTY, EMPTY); \
      push_code(code_list, CODE_MOV, TMP(tmpv), IMM("0"), EMPTY);	\
      push_code(code_list, CODE_JMP, ADDR(donelbl), EMPTY, EMPTY);	\
      push_code(code_list, CODE_LABEL, ADDR(truelbl), EMPTY, EMPTY);	\
      push_code(code_list, CODE_MOV, TMP(tmpv), IMM("1"), EMPTY);	\
      push_code(code_list, CODE_LABEL, ADDR(donelbl), EMPTY, EMPTY);	\
									\
									\
      return tmpv;							\
      break;								\
    }

    CMP_INTERCODE(EQUAL, JE)
      CMP_INTERCODE(GREATER, JA)
      CMP_INTERCODE(LESS, JB)
      CMP_INTERCODE(NOT_EQUAL, JNE)
      CMP_INTERCODE(GREATER_EQUAL, JAE)
      CMP_INTERCODE(LESS_EQUAL, JBE)

  case NODE_IDENTIFIER: {
      tmpvar_t tmpv = make_tmpvar();
      push_code(code_list, CODE_ALLOC_TMP, TMP(tmpv), IMM("8"), EMPTY);
      push_code(code_list, CODE_MOV, TMP(tmpv), VALUE(node->value), EMPTY);
      return tmpv;
      break;
    }
  case NODE_CONSTANT: {
    tmpvar_t tmpv = make_tmpvar();
    push_code(code_list, CODE_ALLOC_TMP, TMP(tmpv), IMM("8"), EMPTY);
    push_code(code_list, CODE_MOV, TMP(tmpv), IMM(node->value), EMPTY);
    return tmpv;
    break;
  }

  case NODE_DECLARE_VAR: {
    case NODE_DECLARE_FUNC: {
      assert(node->left);
      char *name = node->left->value;
      if (node->node_type == NODE_DECLARE_FUNC) {
	name = node->left->left->value;
      }
      push_code(code_list, CODE_EXTERN_DECLARE, KEEP(name), EMPTY, EMPTY);
      break;
    }
  }

  case NODE_BREAK: {
    push_code(code_list, CODE_JMP, ADDR(get_top_while_done_label()), EMPTY,
              EMPTY);
    break;
  }
  case NODE_CLASS:
    break;
  case NODE_CLASSFILL: {
    // alloc tmp, then fill it.
    tmpvar_t tmpvar = make_tmpvar();
    symbol_type_t *symtype = list_get(&type_table, node->value_type);
    push_code(code_list, CODE_ALLOC_TMP, TMP(tmpvar), imm_num(symtype->size), EMPTY);
    // fill the fields
    gen_classfill(code_list, node, &tmpvar);
    return tmpvar;
  }
  case NODE_PROPERTY: {
    tmpvar_t tmpvar = make_tmpvar();
    push_code(code_list, CODE_ALLOC_TMP, TMP(tmpvar), IMM("8"), EMPTY);
    // get the pointer to the object
    char *objectname = node->left->value;
    push_code(code_list, CODE_LOAD, TMP(tmpvar), ADDR(objectname), EMPTY);
    // get the offset of the member
    size_t member_offset = get_member_offset(node->left->value_type, node->right->value);
    push_code(code_list, CODE_LOAD, TMP(tmpvar),
              OFFSETTMP(tmpvar, member_offset), EMPTY);
    return tmpvar;
  }    
  case NODE_FUNCCALL: {
    // gen arglist first
    // keep the tmpvars used to push args used until we finished storing the
    // return value
    list_t tmp_vars = create_list(10, sizeof(tmpvar_t));
    size_t tmpargnum =
      gen_arglist_node(node->right, code_list, tmpnum, layer + 1, &tmp_vars);
    tmpnum += tmpargnum;
    // then call the function
    tmpvar_t rettmpvar = make_tmpvar();
    push_code(code_list, CODE_ALLOC_TMP, TMP(rettmpvar), EMPTY, EMPTY);
    push_code(code_list, CODE_FUNCCALL, ADDR(node->left->value), TMP(rettmpvar),
              EMPTY);
    for (size_t i = 0; i < tmp_vars.len; i++) {
      tmpvar_t *tmpv = (tmpvar_t *)list_get(&tmp_vars, i);
      
    }
    free_list(&tmp_vars);
    return rettmpvar;
    break;
  }
  case NODE_NONE:
  case NODE_ARGLIST:
  case NODE_LEAFHOLDER: {
    if (node->left) {
      gen_node(node->left, code_list, tmpnum, layer);
    }
    if (node->right) {
      gen_node(node->right, code_list, tmpnum, layer);
    }
    break;
  }
  case NODE_SINGLEEXPR: {
    if (node->left) {
      tmpvar_t tmpv = gen_node(node->left, code_list, tmpnum, layer);
      if (!TMPV_EMPTY(tmpv)) {
        
      }
    }
    if (node->right) {
      tmpvar_t tmpv = gen_node(node->right, code_list, tmpnum, layer);
      if (!TMPV_EMPTY(tmpv)) {
        
      }
    }
    break;
  }
  case NODE_ARGPAIR: {
    // left is the name
    tmpvar_t tmpv = gen_node(node->left, code_list, tmpnum, layer);
    push_code(code_list, CODE_PUSHARG, TMP(tmpv), EMPTY, EMPTY);
    
    break;
  }
  case NODE_ELSEIF:
  case NODE_IF: {
    tmpvar_t cond_var = gen_node(node->left, code_list, tmpnum, layer + 1);
    // for comparation nodes, they return 1 if condition is true
    push_code(code_list, CODE_CMP, TMP(cond_var), IMM("1"), EMPTY);
    
    char *true_label = mklabel();
    char *done_label = mklabel();
    // cond_var tmpvar freed
    push_code(code_list, CODE_JE, ADDR(true_label), EMPTY, EMPTY);
    push_code(code_list, CODE_JMP, ADDR(done_label), EMPTY, EMPTY);
    push_code(code_list, CODE_LABEL, ADDR(true_label), EMPTY, EMPTY);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    allocate_syms(code_list, node->syms);
    
    gen_node(node->right, code_list, tmpnum, layer + 1);
    push_code(code_list, CODE_LABEL, ADDR(done_label), EMPTY, EMPTY);
    push_code(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);
    break;
  }
    // todo: while node
  case NODE_ELSE: {
    // this node needs to be called by elseif node or if node.
    // just generate code
    // alloc syms
    allocate_syms(code_list, node->syms);
    
    gen_node(node->left, code_list, tmpnum, layer + 1);
    push_code(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);
    break;
  }
  case NODE_WHILE: {
    char *repeat_label = mklabel();
    push_code(code_list, CODE_LABEL, ADDR(repeat_label), EMPTY, EMPTY);
    tmpvar_t cond_var = gen_node(node->left, code_list, tmpnum, layer + 1);
    // CODE_CMP returns 0 if equ, for comparation nodes, they return 0 if
    // condition is true
    push_code(code_list, CODE_CMP, TMP(cond_var), IMM("0"), EMPTY);
    
    char *true_label = mklabel();
    char *done_label = mklabel();
    // cond_var tmpvar freed
    push_code(code_list, CODE_JE, ADDR(true_label), EMPTY, EMPTY);
    push_code(code_list, CODE_JMP, ADDR(done_label), EMPTY, EMPTY);
    push_code(code_list, CODE_LABEL, ADDR(true_label), EMPTY, EMPTY);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    allocate_syms(code_list, node->syms);
    
    push_while_done_label(done_label);
    gen_node(node->right, code_list, tmpnum, layer + 1);
    pop_while_done_label();
    // judge again
    push_code(code_list, CODE_JMP, ADDR(repeat_label), EMPTY, EMPTY);
    push_code(code_list, CODE_LABEL, ADDR(done_label), EMPTY, EMPTY);
    push_code(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);
    break;
  }
  default:
    cry_errorf(SENDER_INTERCODER, node->position, "%s unsupported",
               get_nodetype_str(node->node_type));
    break;
  }
  return (tmpvar_t){.index=TMPVAR_INDEX_NULL};
}
void put_global_var_inits(list_t *code_list, astnode_t *ast, symbol_table_t* global_level_symtab) {
  int tmpnum = 0;
  if (ast->node_type == NODE_DEFINITION) {
    char *id = NODE_DEF_ID(ast)->value;
    if (find_symbol(global_level_symtab, id)) {
      // this is a global symbol
      tmpvar_t globtmpv = gen_node(ast->right, code_list, tmpnum, 0);
      push_code(code_list, CODE_MOV, VALUE(ast->left->left->value), TMP(globtmpv),
		EMPTY);
      // remove the node since we have generated it already
      // we do this by setting the astnode type to NONE
      /* TODO: use better methods */
      ast->left = NULL;
      ast->right = NULL;
      ast->node_type = NODE_NONE;
    }
  } else {
    if (ast->left && ast->left->layer == 0) {
      put_global_var_inits(code_list, ast->left, global_level_symtab);
    }
    if (ast->right && ast->right->layer == 0) {
      put_global_var_inits(code_list, ast->right, global_level_symtab);
    }
  }
}
list_t gen_intercode(astnode_t *ast) {
  list_t codes = create_list(20, sizeof(intercode_t));
  // generate global symbols
  for (size_t i = 0; i < ast->syms->table.len; ++i) {
    symbol_t *sym = list_get(&ast->syms->table, i);
    if (!sym->is_extern) {
      CODE(&codes, CODE_ALLOC_GLOBAL, ADDR(sym->name), EMPTY, EMPTY);
    }
  }
  // put init code
  CODE(&codes, CODE_DEF_FUNC, ADDR("_start"), EMPTY, EMPTY);
  // scan the ast and find global definitions
  put_global_var_inits(&codes, ast, ast->syms);
  tmpvar_t tmpv = make_tmpvar();
  CODE(&codes, CODE_ALLOC_TMP, TMP(tmpv), EMPTY, EMPTY);
  CODE(&codes, CODE_FUNCCALL, ADDR("main"), TMP(tmpv), EMPTY);
  CODE(&codes, CODE_RETURN, EMPTY, EMPTY, EMPTY);
  gen_node(ast, &codes, 0, 0);
  return codes;
}
// create an operand from an integer.
operand_t imm_num(int value) {
  operand_t op={.type=OPERAND_IMMEDIATE};
  cstring_t cstr = create_string();
  string_sprintf(&cstr, "%d", value);
  op.value = cstr.data;
  // we just use cstring here for convenience. no need to free it.  
  return op;
}
