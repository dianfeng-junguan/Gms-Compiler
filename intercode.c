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
  char *lbl = myalloc(6 + digits + 1);
  assert(lbl);
  memset(lbl, 0, 6 + digits + 1);
  memcpy(lbl, ".label", 6);
  id=labelid++;
  for (size_t i=0; i < digits; ++i) {
    *(lbl + 6 + digits - 1 - i) = '0' + id % 10;
    id/=10;
  }
  return lbl;
}
char* make_tmpvar(size_t tmpvarnum){
  char *tmpv = myalloc(50);
  assert(tmpv);
  memcpy(tmpv, "tmp", 3);                                                  
  size_t tmptmp = tmpvarnum;                                                  
  size_t ptr = 0;                                                          
  while (tmptmp > 0) {                                                     
    *(tmpv + 3 + ptr) = '0' + tmptmp % 10;                                 
    tmptmp /= 10;                                                          
    ptr++;                                                                 
  }
  return tmpv;
}
/// a tool func to create a code on the heap by a local intercode.
intercode_t create_code(intercode_type_t type, char* operand1, char* operand2, char* operand3)			       		
{
  return (intercode_t){
    .type=type,
    .operand1str=operand1?clone_str(operand1):NULL,
    .operand2str=operand2?clone_str(operand2):NULL,
    .operand3str=operand3?clone_str(operand3):NULL,
  };
}
void free_intercode(intercode_t* code){
  FREEIFD(code->operand1str,myfree);
  FREEIFD(code->operand2str,myfree);
  FREEIFD(code->operand3str,myfree);
}
char *gen_node(astnode_t *node, list_t *code_list, int tmpnum, int layer);
size_t gen_arglist_node(astnode_t *node, list_t *code_list, int tmpnum,
                        int layer) {
  size_t num_args=0;
  // used to gen intercode for commalist in NODE_FUNCCALL
  switch (node->node_type) {
  case NODE_COMMALIST: {
    // more args
    num_args+=gen_arglist_node(node->left, code_list, tmpnum, layer);
    num_args+=gen_arglist_node(node->right, code_list, tmpnum, layer);    
    break;
  }
  default:{
    // an arg
    char *arg = gen_node(node, code_list, tmpnum, layer);
    // push the arg
    CODE(code_list, CODE_PUSHARG, arg, 0, 0);
    // one arg    
    num_args=1;
    break;
  }
  }
  return num_args;
}
/* this is used for breaks */
static char *while_done_lbls[32];
static size_t while_done_ptr=0;
void push_while_done_label(char *done_label) {
  while_done_lbls[while_done_ptr++] = done_label;
  assert(while_done_ptr<32);
}
void pop_while_done_label() {
  while_done_ptr--;
  assert(while_done_ptr>=0);
}
char *get_top_while_done_label() {
  if (while_done_ptr == 0)
    return NULL;
  return while_done_lbls[while_done_ptr-1];
}
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
    // first declare the func
    CODE(code_list, CODE_DEF_FUNC,funcname,0,0);
    // then assign the symbols
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      if(sym->layer==layer+1){
	CODE(code_list, CODE_ALLOC_LOCAL, sym->name, "8" ,0);
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
    CODE(code_list, CODE_RETURN, tmpv, 0, 0);
    break;
  }
  case NODE_ASSIGN: {
    assert(node->left&&node->left->value&&node->right);
    char *assigned = node->left->value;
    char* rhs = gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_MOV,assigned,rhs,0);
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
    CODE(code_list, CODE_##type_noprefix, lhs, rhs, res);		       \
    return res;                                                                \
    break;                                                                     \
  }
    
  case NODE_COMMALIST: {
    // return the value of the rightest part
    gen_node(node->left, code_list, tmpnum, layer + 1);
    char *rexpr = gen_node(node->right, code_list, tmpnum, layer + 1);        
    return rexpr;
    break;
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
    
  case NODE_DECLARE_VAR: {
  case NODE_DECLARE_FUNC: {
    assert(node->left);
    CODE(code_list, CODE_EXTERN_DECLARE, node->left->value, 0, 0);
    break;
  }
  }
    

  case NODE_BREAK: {
    CODE(code_list, CODE_JMP, get_top_while_done_label(), 0, 0);    
    break;
  }
  case NODE_FUNCCALL: {
    // gen arglist first
    size_t num_args=0;
    num_args=gen_arglist_node(node->right, code_list, tmpnum,layer+1);
    // then call the function
    CODE(code_list, CODE_FUNCCALL, node->left->value, 0, 0);
    char* rettmpvar=make_tmpvar(tmpnum+1);
    CODE(code_list, CODE_STORE_RETV, rettmpvar, 0, 0);
    return rettmpvar;
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
    push_while_done_label(done_label);
    gen_node(node->right, code_list, tmpnum, layer + 1);
    pop_while_done_label();
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
void put_global_var_inits(list_t *code_list, astnode_t *ast) {
  int tmpnum = 0;
  if (ast->node_type == NODE_DEFINITION && ast->layer == 0) {    
    char* globtmpv=gen_node(ast->right, code_list, tmpnum, 0);
    CODE(code_list, CODE_MOV, ast->left->value, globtmpv, 0);
    // remove the node since we have generated it already
    // we do this by setting the astnode type to NONE
    ast->left=NULL;
    ast->right=NULL;
    ast->node_type=NODE_NONE;
  }else{    
    if(ast->left&&ast->left->layer==0){
      put_global_var_inits(code_list, ast->left);
    }
    if(ast->right&&ast->right->layer==0){
      put_global_var_inits(code_list, ast->right);
    }
  }  
}
list_t gen_intercode(astnode_t* ast){
  list_t codes = create_list(20, sizeof(intercode_t));  
  // generate global symbols
  for (size_t i = 0; i < ast->syms.len; ++i) {
    symbol_t *sym = list_get(&ast->syms, i);
    if(sym->type==SYMBOL_VARIABLE&&!sym->is_extern){
      CODE(&codes, CODE_ALLOC_GLOBAL, sym->name, 0 ,0);
    }
  }
  // put init code
  CODE(&codes, CODE_DEF_FUNC, "_start", 0, 0);  
  // scan the ast and find global definitions
  put_global_var_inits(&codes, ast);
  CODE(&codes, CODE_FUNCCALL, "main", 0, 0);
  CODE(&codes, CODE_RETURN, 0, 0, 0);
  gen_node(ast, &codes,0,0);
  return codes;
}
static char *codetype_strs[] = {
  [CODE_DEF_FUNC] = "CODE_DEF_FUNC",
  [CODE_DEF_FUNC_END] = "CODE_DEF_FUNC_END",
  [CODE_ALLOC_GLOBAL] = "CODE_ALLOC_GLOBAL",
  [CODE_ALLOC_LOCAL] = "CODE_ALLOC_LOCAL",
  [CODE_ALLOC_TMP] = "CODE_ALLOC_TMP",
  [CODE_FREE] = "CODE_FREE",
  [CODE_RETURN] = "CODE_RETURN",
  [CODE_MOV] = "CODE_MOV",

  [CODE_DECLARE]="CODE_DECLARE",
  [CODE_EXTERN_DECLARE]="CODE_EXTERN_DECLARE",    

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
  [CODE_STORE_RETV]="CODE_STORE_RETV",

  
  [CODE_DATA]="CODE_DATA",
  [CODE_DATA_SECTION]="CODE_DATA_SECTION",
  [CODE_TEXT_SECTION]="CODE_TEXT_SECTION",
};
char *codetype_tostr(intercode_type_t type) {
  return codetype_strs[type];
}
