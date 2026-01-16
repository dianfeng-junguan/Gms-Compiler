#include "intercode.h"
#include "err.h"
#include "utils.h"
#include "stdbool.h"
#include "parser.h"
#include "sematic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
/**


 **/
static int labelid = 0;
char* mklabel(){
  int id = labelid;
  size_t digits = 0;
  while (id>0) {
    id /= 10;
    digits++;
  }
  char *lbl = malloc(6 + digits + 1);
  assert(lbl);
  memset(lbl, 0, 6 + digits + 1);
  memcpy(lbl, ".label", 6);
  id=labelid++;
  for (size_t i=0; i < digits; ++i) {
    *(lbl + 6 + i) = '0' + id % 10;
    id/=10;
  }
  return lbl;
}
char* make_tmpvar(size_t tmpvarnum){
  char *tmpv = malloc(50);
  assert(tmpv);
  memcpy(tmpv, "tmp", 3);                                                  
  size_t tmptmp = tmpvarnum;                                                  
  size_t ptr = 0;                                                          
  while (tmptmp > 0) {                                                     
    *(tmpv + 3 + ptr) = '0' + tmptmp % 10;                                 
    tmptmp /= 10;                                                          
    ptr++;                                                                 
  }
  *(tmpv + 3 + ptr) = '\0';
  return tmpv;
}
/// a tool func to create a code on the heap by a local intercode.
intercode_t* create_code(intercode_type_t type, char* operand1, char* operand2, char* operand3)			       		
{								
  intercode_t* __heap_code__ = malloc(sizeof(intercode_t));
  *__heap_code__ =
      (intercode_t){.type = type, .label = operand1, .operand2 = (u64)operand2, .operand3=(u64)operand3};  
  return __heap_code__;
}

#define CODE(code_list, type, op1, op2, op3)                                   \
  list_append(code_list, create_code(type, op1, op2, op3))
/// generate intercode of a node.
/// returns the name of the tmp var which stores the value of the expression if
/// the translated node is an expression
/// format: tmp123
/// args:
/// tmpnum: the num of the tmp vars already in existence
char *gen_node(astnode_t *node, list_t *code_list, int tmpnum, int layer) {
  // reminder: each node should generate at most one temp var.
  switch (node->node_type) {
  case NODE_FUNCTION: {
    assert(node->left&&(node->left->node_type==NODE_IDENTIFIER)&&node->left->value);
    char* funcname=node->left->value;
    intercode_t *code =
        create_code(CODE_DEF_FUNC,funcname,0,0);
    // first declare the func
    list_append(code_list, code);
    // then assign the symbols
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      if(sym->layer==layer+1){
	intercode_t *alloc_local = create_code(CODE_ALLOC_LOCAL, sym->name, "8" ,0);
        list_append(code_list, alloc_local);
      }
    }
    // then gen the body
    gen_node(node->right, code_list, tmpnum, layer + 1);
    CODE(code_list, CODE_DEF_FUNC_END, funcname, 0, 0);
    break;
  }
  case NODE_RETURN: {
    char* tmpv=0;
    if (node->left) {
      // gen the return value first
     tmpv = gen_node(node->left, code_list, tmpnum,layer+1);      
    }
    intercode_t *ret = create_code(CODE_RETURN, tmpv, 0, 0);    
    list_append(code_list, ret);
    break;
  }
  case NODE_ASSIGN: {
    assert(node->left&&node->left->value&&node->right);
    char *assigned = node->left->value;
    char* rhs = gen_node(node->right, code_list, tmpnum,layer+1);
    intercode_t *code = create_code(CODE_MOV,assigned,rhs,0);
    list_append(code_list, code);
    break;
  }
  case NODE_DEFINITION: {
    char* rhs=gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_MOV, node->left->value, rhs, 0);
    break;
  }
#define TWOOP_INTERCODE(type_noprefix)                                         \
  case NODE_##type_noprefix: {                                                 \
    assert(node->left && node->right);                                         \
    char *lhs = gen_node(node->left, code_list, tmpnum, layer + 1);            \
    char *rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 2);       \
    char *res = make_tmpvar(tmpnum + 3);                                       \
    CODE(code_list, CODE_ALLOC_TMP, res, "8", 0);                              \
    intercode_t *code = create_code(CODE_##type_noprefix, lhs, rhs, res);      \
    list_append(code_list, code);                                              \
    return res;                                                                \
    break;                                                                     \
  }    
    
    TWOOP_INTERCODE(ADD)
      TWOOP_INTERCODE(SUB)
      TWOOP_INTERCODE(MUL)
      TWOOP_INTERCODE(DIV)
      TWOOP_INTERCODE(MOD)
      
      TWOOP_INTERCODE(BITAND)
      TWOOP_INTERCODE(BITOR)
      // TWOOP_INTERCODE(BITNOT)
      /*
	the comparing node returns 1 if true, otherwise 0 as false.
       */
#define CMP_INTERCODE(type_noprefix, jmpcode)                                  \
  case NODE_##type_noprefix: {                                                 \
      assert(node->left && node->right);                                       \
      char *lhs = gen_node(node->left, code_list, tmpnum, layer + 1);          \
      char *rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 1);     \
      char *tmpv = make_tmpvar(tmpnum + 2);                                    \
      CODE(code_list, CODE_CMP, lhs, rhs, 0);                                  \
      char *donelbl = mklabel();                                               \
      char *truelbl = mklabel();                                               \
      CODE(code_list, CODE_##jmpcode, truelbl, 0, 0);                          \
      CODE(code_list, CODE_MOV, tmpv, "0", 0);                                 \
      CODE(code_list, CODE_JMP, donelbl, 0, 0);                                \
      CODE(code_list, CODE_LABEL, truelbl, 0, 0);                              \
      CODE(code_list, CODE_MOV, tmpv, "1", 0);                                 \
      CODE(code_list, CODE_LABEL, donelbl, 0, 0);                              \
      return tmpv;                                                             \
      break;                                                                   \
  }
     
      CMP_INTERCODE(EQUAL, JE)
      CMP_INTERCODE(GREATER, JA)
      CMP_INTERCODE(LESS, JB)
      CMP_INTERCODE(NOT_EQUAL, JNE)
      CMP_INTERCODE(GREATER_EQUAL, JAE)
      CMP_INTERCODE(LESS_EQUAL, JBE)
      
  case NODE_IDENTIFIER: {
      return node->value;
      break;
  }
  case NODE_CONSTANT: {
    char *tmpv = make_tmpvar(tmpnum + 1);
    CODE(code_list, CODE_ALLOC_TMP, tmpv, "8", 0);    
    CODE(code_list, CODE_MOV, tmpv, node->value, 0);
    return tmpv;
    break;
  }
  case NODE_FUNCCALL: {
    // gen arglist first
    gen_node(node->right, code_list, tmpnum,layer+1);
    // then call the function
    CODE(code_list, CODE_FUNCCALL, node->left->value, 0, 0);    
    break;
  }
  case NODE_NONE:case NODE_ARGLIST: 
  case NODE_LEAFHOLDER: {
    if(node->left){
      gen_node(node->left, code_list, tmpnum,layer);
    }
    if(node->right){
      gen_node(node->right, code_list, tmpnum,layer);
    }    
    break;
  }
  case NODE_ARGPAIR: {
    // left is the name
    CODE(code_list, CODE_PUSHARG, node->left->value, 0, 0);
    break;
  }
  case NODE_ELSEIF:
  case NODE_IF: {
    char *cond_var = gen_node(node->left, code_list, tmpnum,layer+1);
    // for comparation nodes, they return 1 if condition is true
    CODE(code_list, CODE_CMP, cond_var, "1", 0);
    char* true_label=mklabel();
    char* done_label=mklabel();
    // cond_var tmpvar freed
    CODE(code_list, CODE_JE, true_label, 0, 0);
    CODE(code_list, CODE_JMP, done_label, 0, 0);
    CODE(code_list, CODE_LABEL, true_label, 0, 0);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      //printf("sym: name=%s, layer=%d, cur layer=%d\n",sym->name, sym->layer, layer);
      if (sym->layer==layer+1)
	CODE(code_list, CODE_ALLOC_LOCAL, sym->name, "8" ,0);
    }
    gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_LABEL, done_label, 0, 0);
    CODE(code_list, CODE_SCOPE_END, 0, 0, 0);    
    break;
  }
    // todo: while node
  case NODE_ELSE: {
    // this node needs to be called by elseif node or if node.
    // just generate code
    // alloc syms
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      if (sym->layer==layer+1)
	CODE(code_list, CODE_ALLOC_LOCAL, sym->name, "8" ,0);
    }
    gen_node(node->left, code_list, tmpnum, layer + 1);
    CODE(code_list, CODE_SCOPE_END, 0, 0, 0);    
    break;
  }
  case NODE_WHILE: {
    char *repeat_label=mklabel();
    CODE(code_list, CODE_LABEL, repeat_label, 0, 0);    
    char *cond_var = gen_node(node->left, code_list, tmpnum,layer+1);
    // CODE_CMP returns 0 if equ, for comparation nodes, they return 0 if condition is true
    CODE(code_list, CODE_CMP, cond_var, "1", 0);
    char* true_label=mklabel();
    char* done_label=mklabel();
    // cond_var tmpvar freed
    CODE(code_list, CODE_JE, true_label, 0, 0);
    CODE(code_list, CODE_JMP, done_label, 0, 0);
    CODE(code_list, CODE_LABEL, true_label, 0, 0);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      //printf("sym: name=%s, layer=%d, cur layer=%d\n",sym->name, sym->layer, layer);
      if (sym->layer==layer+1)
	CODE(code_list, CODE_ALLOC_LOCAL, sym->name, "8" ,0);
    }
    gen_node(node->right, code_list, tmpnum, layer + 1);
    // judge again
    CODE(code_list, CODE_JMP, repeat_label, 0, 0);
    CODE(code_list, CODE_LABEL, done_label, 0, 0);
    CODE(code_list, CODE_SCOPE_END, 0, 0, 0);    
    break;
  }
  default:
    cry_errorf(SENDER_INTERCODER, node->position, "%s unsupported", get_nodetype_str(node->node_type));
    break;
  }
  return NULL;
}

list_t gen_intercode(astnode_t* ast){
  list_t codes=create_list(20, sizeof(intercode_t));
  gen_node(ast, &codes,0,0);
  return codes;
}
static char *codetype_strs[] = {
  [CODE_DEF_FUNC]="CODE_DEF_FUNC",
  [CODE_DEF_FUNC_END]="CODE_DEF_FUNC_END",
  [CODE_ALLOC_GLOBAL]="CODE_ALLOC_GLOBAL",
  [CODE_ALLOC_LOCAL]="CODE_ALLOC_LOCAL",
  [CODE_ALLOC_TMP]="CODE_ALLOC_TMP",
  [CODE_FREE]="CODE_FREE",
  [CODE_RETURN]="CODE_RETURN",
  [CODE_MOV]="CODE_MOV",

  // operators
  [CODE_ADD]="CODE_ADD",
  [CODE_SUB]="CODE_SUB",
  [CODE_MUL]="CODE_MUL",
  [CODE_DIV]="CODE_DIV",
  [CODE_MOD]="CODE_MOD",
  [CODE_BITAND]="CODE_BITAND",
  [CODE_BITOR]="CODE_BITOR",
  [CODE_BITNOT]="CODE_BITNOT",
  // compare
  [CODE_CMP]="CODE_CMP",
  // jmping
  [CODE_JMP]="CODE_JMP",
  [CODE_JA]="CODE_JA",
  [CODE_JAE]="CODE_JAE",
  [CODE_JB]="CODE_JB",
  [CODE_JBE]="CODE_JBE",
  [CODE_JE]="CODE_JE",
  [CODE_JNE]="CODE_JNE",
  [CODE_LABEL]="CODE_LABEL",
  [CODE_SCOPE_END]="CODE_SCOPE_END",
  [CODE_PUSHARG]="CODE_PUSHARG",
  [CODE_FUNCCALL]="CODE_FUNCCALL",
};
char *codetype_tostr(intercode_type_t type) {
  return codetype_strs[type];
}
