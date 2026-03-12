#include "status.h"
#include "utils.h"
#include "parser.h"
void init_compiler_global_data(compiler_global_data_t *globals) {
  // FIXME: when symtab reallocs its data, node->syms will become a wild pointer bc it is a pointer referring the freed memory  
  globals->symbol_tables=create_list(100, sizeof(symbol_table_t));
}
