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
  int digits=(id==0?1:0);
  while(id>0){
    id/=10;
    digits++;
  }
  char* number=malloc(digits+1);
  sprintf(number,"%d",labelid);
  char* lbl=malloc(5+digits+1);
  strcpy(lbl, "label");
  lbl = strcat(lbl, number);
  labelid++;
  return lbl;
}
char* make_tmpvar(size_t tmpvarnum){
  char *tmpv = myalloc(50);
  assert(tmpv);
  memcpy(tmpv, "@tmp", 4);                                                  
  size_t tmptmp = tmpvarnum;                                                  
  size_t ptr = 0;                                                          
  while (tmptmp > 0) {                                                     
    *(tmpv + 4 + ptr) = '0' + tmptmp % 10;                                 
    tmptmp /= 10;                                                          
    ptr++;                                                                 
  }
  return tmpv;
}
/// a tool func to create a code on the heap by a local intercode.
intercode_t create_code(intercode_type_t type, operand_t operand1, operand_t operand2, operand_t operand3)			       		
{
  return (intercode_t){
    .type=type,
    .op1=operand1,
    .op2=operand2,
    .op3=operand3,
  };
}
void free_intercode(intercode_t* code){
  /* TODO: judge the operand type and free*/
  
  
}
char *gen_node(astnode_t *node, list_t *code_list, int tmpnum, int layer);
size_t gen_arglist_node(astnode_t *node, list_t *code_list, int tmpnum,
                        int layer, list_t *tmpvars) {
  size_t num_args=0;
  // used to gen intercode for commalist in NODE_FUNCCALL
  switch (node->node_type) {
  case NODE_COMMALIST: {
    // more args
    num_args+=gen_arglist_node(node->left, code_list, tmpnum, layer,tmpvars);
    num_args+=gen_arglist_node(node->right, code_list, tmpnum, layer,tmpvars);    
    break;
  }
  default:{
    // an arg
    char *arg = gen_node(node, code_list, tmpnum, layer);
    append(tmpvars, &arg);
    // push the arg
    CODE(code_list, CODE_PUSHARG, TMP(arg), EMPTY, EMPTY);
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
    assert(node->left&&(node->left->left->node_type==NODE_IDENTIFIER)&&node->left->left->value);
    char* funcname=node->left->left->value;
    // first declare the func
    CODE(code_list, CODE_DEF_FUNC,KEEP(funcname),EMPTY,EMPTY);
    // then assign the symbols
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      if(sym->layer==layer+1){
	CODE(code_list, CODE_ALLOC_LOCAL, KEEP(sym->name), IMM("8") ,EMPTY);
      }
    }
    // then gen the body
    gen_node(node->right, code_list, tmpnum, layer + 1);
    CODE(code_list, CODE_DEF_FUNC_END, KEEP(funcname), EMPTY, EMPTY);
    break;
  }
  case NODE_RETURN: {
    char* tmpv=0;
    if (node->left) {
      // gen the return value first
     tmpv = gen_node(node->left, code_list, tmpnum,layer+1);      
    }
    CODE(code_list, CODE_RETURN, TMP(tmpv), EMPTY, EMPTY);
    CODE(code_list, CODE_FREE, TMP(tmpv), EMPTY, EMPTY);
    break;
  }
  case NODE_ASSIGN: {
    assert(node->left&&node->left->value&&node->right);
    char *assigned = node->left->value;
    char* rhs = gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_MOV,VALUE(assigned),TMP(rhs),EMPTY);
    CODE(code_list, CODE_FREE, TMP(rhs), EMPTY, EMPTY);
    break;
  }
  case NODE_DEFINITION: {
    char* rhs=gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_MOV, VALUE(node->left->value), TMP(rhs), EMPTY);
    CODE(code_list, CODE_FREE, TMP(rhs), EMPTY, EMPTY);
    break;
  }
#define TWOOP_INTERCODE(type_noprefix)                                         \
  case NODE_##type_noprefix: {                                                 \
    assert(node->left && node->right);                                         \
    char *lhs = gen_node(node->left, code_list, tmpnum, layer + 1);            \
    char *rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 2);       \
    char *res = make_tmpvar(tmpnum + 3);                                       \
    CODE(code_list, CODE_ALLOC_TMP, TMP(res), IMM("8"), EMPTY);                \
    CODE(code_list, CODE_##type_noprefix, TMP(lhs), TMP(rhs), TMP(res));       \
    CODE(code_list, CODE_FREE, TMP(lhs), EMPTY, EMPTY);                        \
    CODE(code_list, CODE_FREE, TMP(rhs), EMPTY, EMPTY);                        \
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
    
    TWOOP_INTERCODE(ADD);
    TWOOP_INTERCODE(SUB);
    TWOOP_INTERCODE(MUL);
    TWOOP_INTERCODE(DIV);
    TWOOP_INTERCODE(MOD);
      
    TWOOP_INTERCODE(BITAND);
    TWOOP_INTERCODE(BITOR);

  case NODE_REFER:{
    char* res=make_tmpvar(tmpnum+1);
    CODE(code_list, CODE_REFER, TMP(res), ADDR(node->right->value), EMPTY);
    return res;
    break;
  }
  case NODE_DEFER:{
    char* res=make_tmpvar(tmpnum+1);
    CODE(code_list, CODE_DEFER, TMP(res), ADDR(node->right->value), EMPTY);
    return res;
    break;
  }
      // TWOOP_INTERCODE(BITNOT)
      /*
	the comparing node returns 1 if true, otherwise 0 as false.
       */
#define CMP_INTERCODE(type_noprefix, jmpcode)                                  \
  case NODE_##type_noprefix: {                                                 \
      assert(node->left && node->right);                                       \
      char *lhs = gen_node(node->left, code_list, tmpnum, layer + 1);          \
      char *rhs = gen_node(node->right, code_list, tmpnum + 1, layer + 1);     \
      char *tmpv = make_tmpvar(tmpnum + 2);				\
      CODE(code_list, CODE_CMP, TMP(lhs), TMP(rhs), EMPTY);		\
      char *donelbl = mklabel();					\
      char *truelbl = mklabel();					\
      CODE(code_list, CODE_##jmpcode, ADDR(truelbl), EMPTY, EMPTY);	\
      CODE(code_list, CODE_MOV, TMP(tmpv), IMM("0"), EMPTY);		\
      CODE(code_list, CODE_JMP, ADDR(donelbl), EMPTY, EMPTY);		\
      CODE(code_list, CODE_LABEL, ADDR(truelbl), EMPTY, EMPTY);		\
      CODE(code_list, CODE_MOV, TMP(tmpv), IMM("1"), EMPTY);		\
      CODE(code_list, CODE_LABEL, ADDR(donelbl), EMPTY, EMPTY);		\
      CODE(code_list, CODE_FREE, TMP(lhs), EMPTY, EMPTY);		\
      CODE(code_list, CODE_FREE, TMP(rhs), EMPTY, EMPTY);		\
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
      char *tmpv = make_tmpvar(tmpnum + 1);
      CODE(code_list, CODE_ALLOC_TMP, TMP(tmpv), IMM("8"), EMPTY);    
      CODE(code_list, CODE_MOV, TMP(tmpv), VALUE(node->value), EMPTY);
      return tmpv;
      break;
  }
  case NODE_CONSTANT: {
    char *tmpv = make_tmpvar(tmpnum + 1);
    CODE(code_list, CODE_ALLOC_TMP, TMP(tmpv), IMM("8"), EMPTY);    
    CODE(code_list, CODE_MOV, TMP(tmpv), IMM(node->value), EMPTY);
    return tmpv;
    break;
  }
    
  case NODE_DECLARE_VAR: {
    case NODE_DECLARE_FUNC: {
      assert(node->left);
      char *name = node->left->value;
      if(node->node_type==NODE_DECLARE_FUNC){
	name=node->left->left->value;
      }
      CODE(code_list, CODE_EXTERN_DECLARE, KEEP(name), EMPTY, EMPTY);
      break;
    }
  }
    

  case NODE_BREAK: {
    CODE(code_list, CODE_JMP, ADDR(get_top_while_done_label()), EMPTY, EMPTY);    
    break;
  }
  case NODE_FUNCCALL: {
    // gen arglist first
    // keep the tmpvars used to push args used until we finished storing the return value
    list_t tmp_vars=create_list(10, sizeof(char*));
    size_t tmpargnum=gen_arglist_node(node->right, code_list, tmpnum,layer+1,&tmp_vars);
    tmpnum+=tmpargnum;
    // then call the function
    char* rettmpvar=make_tmpvar(tmpnum+1);
    CODE(code_list, CODE_ALLOC_TMP, TMP(rettmpvar), EMPTY, EMPTY);
    CODE(code_list, CODE_FUNCCALL, ADDR(node->left->value), TMP(rettmpvar), EMPTY);
    for (size_t i=0; i<tmp_vars.len; i++) {
      char* tmpv=*(char**)list_get(&tmp_vars, i);
      CODE(code_list, CODE_FREE, TMP(tmpv), EMPTY, EMPTY);
      // free the generated tmpv string
      myfree(tmpv);
    }
    free_list(&tmp_vars);
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
  case NODE_SINGLEEXPR:{
    if(node->left){
      char* tmpv=gen_node(node->left, code_list, tmpnum,layer);
      if(tmpv){
	CODE(code_list, CODE_FREE, TMP(tmpv), EMPTY, EMPTY);
      }
    }
    if(node->right){
      char* tmpv=gen_node(node->right, code_list, tmpnum,layer);
      if(tmpv){
	CODE(code_list, CODE_FREE, TMP(tmpv), EMPTY, EMPTY);
      }
    }
    break;
  }
  case NODE_ARGPAIR: {
    // left is the name
    char* tmpv=gen_node(node->left, code_list, tmpnum, layer);
    CODE(code_list, CODE_PUSHARG, TMP(tmpv), EMPTY, EMPTY);
    CODE(code_list, CODE_FREE, TMP(tmpv), EMPTY, EMPTY);
    break;
  }
  case NODE_ELSEIF:
  case NODE_IF: {
    char *cond_var = gen_node(node->left, code_list, tmpnum,layer+1);
    // for comparation nodes, they return 1 if condition is true
    CODE(code_list, CODE_CMP, TMP(cond_var), IMM("1"), EMPTY);
    CODE(code_list, CODE_FREE, TMP(cond_var), EMPTY, EMPTY);
    char* true_label=mklabel();
    char* done_label=mklabel();
    // cond_var tmpvar freed
    CODE(code_list, CODE_JE, ADDR(true_label), EMPTY, EMPTY);
    CODE(code_list, CODE_JMP,ADDR(done_label), EMPTY, EMPTY);
    CODE(code_list, CODE_LABEL,ADDR(true_label), EMPTY, EMPTY);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      //printf("sym: name=%s, layer=%d, cur layer=%d\n",sym->name, sym->layer, layer);
      if (sym->layer==layer+1)
	CODE(code_list, CODE_ALLOC_LOCAL, KEEP(sym->name), IMM("8") ,EMPTY);
    }
    gen_node(node->right, code_list, tmpnum,layer+1);
    CODE(code_list, CODE_LABEL, ADDR(done_label), EMPTY, EMPTY);
    CODE(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);    
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
	CODE(code_list, CODE_ALLOC_LOCAL, KEEP(sym->name), IMM("8") ,EMPTY);
    }
    gen_node(node->left, code_list, tmpnum, layer + 1);
    CODE(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);    
    break;
  }
  case NODE_WHILE: {
    char *repeat_label=mklabel();
    CODE(code_list, CODE_LABEL, ADDR(repeat_label), EMPTY, EMPTY);    
    char *cond_var = gen_node(node->left, code_list, tmpnum,layer+1);
    // CODE_CMP returns 0 if equ, for comparation nodes, they return 0 if condition is true
    CODE(code_list, CODE_CMP, TMP(cond_var), IMM("0"), EMPTY);
    CODE(code_list, CODE_FREE, TMP(cond_var), EMPTY, EMPTY);
    char* true_label=mklabel();
    char* done_label=mklabel();
    // cond_var tmpvar freed
    CODE(code_list, CODE_JE, ADDR(true_label), EMPTY, EMPTY);
    CODE(code_list, CODE_JMP,ADDR(done_label), EMPTY, EMPTY);
    CODE(code_list, CODE_LABEL,ADDR(true_label), EMPTY, EMPTY);
    // body
    // right: leafholder(statements, elseif/else/NULL)
    // alloc syms
    for (size_t i=0; i < node->syms.len; ++i) {
      symbol_t *sym = list_get(&node->syms, i);
      //printf("sym: name=%s, layer=%d, cur layer=%d\n",sym->name, sym->layer, layer);
      if (sym->layer==layer+1)
	CODE(code_list, CODE_ALLOC_LOCAL, ADDR(sym->name), IMM("8") ,EMPTY);
    }
    push_while_done_label(done_label);
    gen_node(node->right, code_list, tmpnum, layer + 1);
    pop_while_done_label();
    // judge again
    CODE(code_list, CODE_JMP, ADDR(repeat_label), EMPTY, EMPTY);
    CODE(code_list, CODE_LABEL,ADDR(done_label), EMPTY, EMPTY);
    CODE(code_list, CODE_SCOPE_END, EMPTY, EMPTY, EMPTY);    
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
    CODE(code_list, CODE_MOV, VALUE(ast->left->value), TMP(globtmpv), EMPTY);
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
      CODE(&codes, CODE_ALLOC_GLOBAL, ADDR(sym->name), EMPTY ,EMPTY);
    }
  }
  // put init code
  CODE(&codes, CODE_DEF_FUNC, ADDR("_start"), EMPTY, EMPTY);  
  // scan the ast and find global definitions
  put_global_var_inits(&codes, ast);
  char *tmpv = make_tmpvar(0);
  CODE(&codes, CODE_ALLOC_TMP, TMP(tmpv), EMPTY, EMPTY);
  CODE(&codes, CODE_FUNCCALL, ADDR("main"), TMP(tmpv), EMPTY);
  CODE(&codes, CODE_RETURN, EMPTY, EMPTY, EMPTY);
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
  
  [CODE_REFER]="CODE_REFER",
  [CODE_DEFER]="CODE_DEFER",
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
  //
  [CODE_LOAD]="CODE_LOAD",
  [CODE_STORE]="CODE_STORE",
  [CODE_M2M]="CODE_M2M",
};
char *codetype_tostr(intercode_type_t type) {
  return codetype_strs[type];
}
