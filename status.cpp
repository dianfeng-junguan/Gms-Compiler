#include "status.h"
void init_compiler_global_data(compiler_global_data_t *globals) {
  // FIXME: when symtab reallocs its data, node->syms will become a wild pointer bc it is a pointer referring the freed memory  
  // globals->symbol_tables=list_create(100, sizeof(symbol_table_t));
}
void free_compiler_global_data(compiler_global_data_t *globals){
  // for(int i=0;i<globals->symbol_tables.len;i++){
  //   symbol_table_t* symtab=(symbol_table_t*)list_get(&globals->symbol_tables,i);
  //   if(symtab){
  //     free_symtab(symtab);
  //   }
  // }
  // free_list(&globals->symbol_tables);
}