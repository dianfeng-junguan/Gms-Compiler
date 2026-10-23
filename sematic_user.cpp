#include "parser.h"
#include "sematic.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct info {
  std::string type;
  bool callable = false;
  std::vector<std::string> args;
  std::string result = "void";
};
struct state {
  std::unordered_map<sematic_symbol_id_t, info> symbols;
  int flow = 0;
};
static std::unordered_map<sematic_context_t *, state> states;
static state &S(sematic_context_t *c) { return states[c]; }
static std::string V(token_t *t) { return t ? t->value : ""; }
static bool bad(sematic_context_t *c, const std::string &m) {
  if (c)
    c->diagnostics.push_back(m);
  return false;
}
static void setty(sematic_context_t *c, ast_node_t *n, const std::string &t) {
  if (c && n) {
    c->type_by_node.erase(n);
    c->type_by_node.emplace(n, type_t(t));
  }
}
static std::string TY(sematic_context_t *c, ast_node_t *n) {
  if (!c || !n)
    return "unknown";
  auto i = c->type_by_node.find(n);
  return i == c->type_by_node.end() ? "unknown" : i->second.name;
}
static bool num(const std::string &t) {
  return t == "int" || t == "char" || t == "bool";
}
static bool compat(const std::string &a, const std::string &b) {
  return a == b || (num(a) && num(b)) || (a == "pointer" && b == "string") ||
         (a == "string" && b == "pointer");
}
static std::string type_name(composed_type_t *n) {
  if (!n)
    return "void";
  switch (n->get_kind()) {
  case NODE_COMPOSED_TYPE_RAW_TYPE:
    return V(static_cast<composed_type_raw_type_t *>(n)->inner_type);
  case NODE_COMPOSED_TYPE_PAREN:
    return type_name(static_cast<composed_type_paren_t *>(n)->inner_type);
  case NODE_COMPOSED_TYPE_PTR:
    return "pointer";
  case NODE_COMPOSED_TYPE_SIZEDARR:
  case NODE_COMPOSED_TYPE_NSIZEDARR:
    return "array";
  default:
    return "unknown";
  }
}
static std::string return_name(func_returntype_t *r) {
  if (!r || r->get_kind() == NODE_FUNC_RETURNTYPE_VOID)
    return "void";
  return type_name(static_cast<func_returntype_hastype_t *>(r)->return_type);
}
static bool enter(sematic_context_t *c, ast_node_t *n) {
  if (!c || !n)
    return false;
  c->push_symbol_table(n);
  c->scope_by_node[n] = c->current_symbol_table_index;
  return true;
}
static void bind(sematic_context_t *c, ast_node_t *n, token_t *t,
                 symbol_type_t k, const std::string &type) {
  if (!c || !t)
    return;
  auto r = c->add_symbol(V(t), k);
  if (r.valid()) {
    c->bind_symbol(n, r);
    S(c).symbols[r.symbol_id] = {type, false, {}, "void"};
  }
}
static bool binary(sematic_context_t *c, ast_node_t *n, ast_node_t *l,
                   ast_node_t *r) {
  auto a = TY(c, l), b = TY(c, r);
  if (num(a) && num(b)) {
    setty(c, n, "int");
    return true;
  }
  return bad(c, "invalid operands: " + a + " and " + b);
}
} // namespace

bool resolve_symbol(ast_node_t *, token_t *, sematic_context_t *);

bool predeclare_function(ast_node_t *n, token_t *t, parameter_list_t *,
                         func_returntype_t *rty, sematic_context_t *c) {
  if (!c || !t)
    return false;
  auto r = c->add_symbol(V(t), symbol_type_t::function);
  if (!r.valid())
    return bad(c, "duplicate function: " + V(t));
  c->bind_symbol(n, r);
  S(c).symbols[r.symbol_id] = {"function", true, {}, return_name(rty)};
  return true;
}
bool predeclare_struct(ast_node_t *n, token_t *t, sematic_context_t *c) {
  if (!c || !t)
    return false;
  auto r = c->add_symbol(V(t), symbol_type_t::struct_type);
  if (!r.valid())
    return bad(c, "duplicate struct: " + V(t));
  c->bind_symbol(n, r);
  S(c).symbols[r.symbol_id] = {V(t), false, {}, "void"};
  return true;
}
bool create_function_scope(ast_node_t *n, sematic_context_t *c) {
  return enter(c, n);
}
bool create_struct_scope(ast_node_t *n, sematic_context_t *c) {
  return enter(c, n);
}
bool create_block_scope(ast_node_t *n, sematic_context_t *c) {
  return enter(c, n);
}
bool create_loop_scope(ast_node_t *n, sematic_context_t *c) {
  return enter(c, n);
}
bool leave_scope(ast_node_t *, sematic_context_t *c) {
  return c && c->leave_symbol_table();
}
bool predeclare_parameters(ast_node_t *n, parameter_list_t *p,
                           sematic_context_t *c) {
  if (!p || p->get_kind() == NODE_PARAMETER_LIST_EMPTY)
    return true;
  auto x = static_cast<parameter_list_hasarg_t *>(p)->args;
  while (x) {
    if (x->get_kind() == NODE_PARAMETER_LIST_NONEMPTY_SINGARG) {
      auto a = static_cast<parameter_list_nonempty_singarg_t *>(x);
      bind(c, n, a->name, symbol_type_t::variable, type_name(a->arg_type));
      break;
    }
    auto a = static_cast<parameter_list_nonempty_multiarg_t *>(x);
    bind(c, n, a->name, symbol_type_t::variable, type_name(a->arg_type));
    x = a->other_args;
  }
  return true;
}
bool enter_function_scope(ast_node_t *n, sematic_context_t *c) {
  return c && c->enter_symbol_table(n);
}
bool enter_struct_scope(ast_node_t *n, sematic_context_t *c) {
  return c && c->enter_symbol_table(n);
}
bool enter_block_scope(ast_node_t *n, sematic_context_t *c) {
  return c && c->enter_symbol_table(n);
}
bool enter_loop_scope(ast_node_t *n, sematic_context_t *c) {
  return c && c->enter_symbol_table(n);
}
bool declare_parameters(ast_node_t *n, parameter_list_t *p,
                        sematic_context_t *c) {
  return predeclare_parameters(n, p, c);
}
static bool members(ast_node_t *n, structmembers_t *m, sematic_context_t *c) {
  if (!m || m->get_kind() == NODE_STRUCTMEMBERS_EMPTY)
    return true;
  if (m->get_kind() == NODE_STRUCTMEMBERS_SINGMEM) {
    auto x = static_cast<structmembers_singmem_t *>(m);
    bind(c, n, x->name, symbol_type_t::variable, type_name(x->def_type));
    return true;
  }
  auto x = static_cast<structmembers_multimem_t *>(m);
  bind(c, n, x->name, symbol_type_t::variable, type_name(x->def_type));
  return members(n, x->other_members, c);
}
bool predeclare_members(ast_node_t *n, structmembers_t *m,
                        sematic_context_t *c) {
  return members(n, m, c);
}
bool declare_members(ast_node_t *n, structmembers_t *m, sematic_context_t *c) {
  return members(n, m, c);
}
bool declare_variable(ast_node_t *n, token_t *t, composed_type_t *q,
                      sematic_context_t *c) {
  if (!q)
    return false;
  bind(c, n, t, symbol_type_t::variable, type_name(q));
  return true;
}
bool declare_inferred_variable(ast_node_t *n, token_t *t, ultimate_expr_t *e,
                               sematic_context_t *c) {
  bind(c, n, t, symbol_type_t::variable, TY(c, e));
  return true;
}
bool declare_variable(ast_node_t *n, token_t *t, sematic_context_t *c) {
  bind(c, n, t, symbol_type_t::variable, "int");
  return true;
}
bool resolve_function_signature(ast_node_t *n, token_t *t, parameter_list_t *,
                                func_returntype_t *, sematic_context_t *c) {
  return resolve_symbol(n, t, c);
}
bool resolve_struct_signature(ast_node_t *n, token_t *t, structmembers_t *,
                              sematic_context_t *c) {
  return resolve_symbol(n, t, c);
}
bool resolve_arguments(ast_node_t *, argument_list_t *, sematic_context_t *) {
  return true;
}
bool resolve_member(ast_node_t *n, lvalue_t *, token_t *t,
                    sematic_context_t *c) {
  return resolve_symbol(n, t, c);
}
bool resolve_symbol(ast_node_t *n, token_t *t, sematic_context_t *c) {
  if (!c || !t)
    return false;
  auto r = c->lookup_symbol(V(t));
  return r.valid() ? c->bind_symbol(n, r) : bad(c, "unknown symbol: " + V(t));
}
bool enter_function_context(ast_node_t *, token_t *t, func_returntype_t *,
                            sematic_context_t *c) {
  if (!c || !t)
    return false;
  auto r = c->lookup_symbol(V(t));
  if (!r.valid())
    return bad(c, "unknown function: " + V(t));
  c->function_stack.push_back(r);
  S(c).flow = 0;
  return true;
}
bool check_function_type(ast_node_t *, parameter_list_t *, func_returntype_t *,
                         sematic_context_t *) {
  return true;
}
bool leave_function_context(ast_node_t *, sematic_context_t *c) {
  if (!c || c->function_stack.empty())
    return false;
  c->function_stack.pop_back();
  S(c).flow = 0;
  return true;
}
bool enter_struct_context(ast_node_t *, token_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool leave_struct_context(ast_node_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool check_valid_type(ast_node_t *n, composed_type_t *q, sematic_context_t *c) {
  auto t = type_name(q);
  if (t == "unknown" || t == "void")
    return bad(c, "invalid type");
  setty(c, n, t);
  return true;
}
bool require_valid_variable_type(ast_node_t *n, composed_type_t *q,
                                 sematic_context_t *c) {
  return check_valid_type(n, q, c);
}
bool require_valid_parameter_type(ast_node_t *n, composed_type_t *q,
                                  sematic_context_t *c) {
  return check_valid_type(n, q, c);
}
bool require_valid_member_type(ast_node_t *n, composed_type_t *q,
                               sematic_context_t *c) {
  return check_valid_type(n, q, c);
}
bool check_assignable(ast_node_t *n, composed_type_t *q, ultimate_expr_t *e,
                      sematic_context_t *c) {
  return compat(type_name(q), TY(c, e)) || bad(c, "incompatible assignment");
}
bool infer_variable_type(ast_node_t *n, token_t *, ultimate_expr_t *e,
                         sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool require_modifiable_lvalue(ast_node_t *, lvalue_t *, sematic_context_t *) {
  return true;
}
bool check_assignable(ast_node_t *n, lvalue_t *l, assign_expr_t *e,
                      sematic_context_t *c) {
  return compat(TY(c, l), TY(c, e)) || bad(c, "incompatible assignment");
}
bool propagate_type(ast_node_t *n, lvalue_t *l, sematic_context_t *c) {
  setty(c, n, TY(c, l));
  return true;
}
#define LVOP(F)                                                                \
  bool F(ast_node_t *n, lvalue_t *l, assign_expr_t *r, sematic_context_t *c) { \
    return binary(c, n, l, r);                                                 \
  }
LVOP(check_addable)
LVOP(check_minusable) LVOP(check_mulable) LVOP(check_divable)
    LVOP(check_modable) LVOP(check_bitandable) LVOP(check_bitorable)
        LVOP(check_shiftleftable)
            LVOP(check_shiftrightable) bool propagate_type(
                ast_node_t *n, logic_expr_or_t *, sematic_context_t *c) {
  setty(c, n, "bool");
  return true;
}
bool check_equalable(ast_node_t *n, logic_expr_eq_t *, bitor_expr_t *,
                     sematic_context_t *c) {
  setty(c, n, "bool");
  return true;
}
bool set_boolean_type(ast_node_t *n, sematic_context_t *c) {
  setty(c, n, "bool");
  return true;
}
bool check_nequalable(ast_node_t *n, logic_expr_eq_t *, bitor_expr_t *,
                      sematic_context_t *c) {
  return set_boolean_type(n, c);
}
bool check_comparable(ast_node_t *n, logic_expr_eq_t *, bitor_expr_t *,
                      sematic_context_t *c) {
  return set_boolean_type(n, c);
}
bool require_boolean(ast_node_t *, logic_expr_or_t *, sematic_context_t *) {
  return true;
}
bool require_boolean(ast_node_t *, logic_expr_and_t *, sematic_context_t *) {
  return true;
}
bool require_boolean(ast_node_t *, logic_expr_eq_t *, sematic_context_t *) {
  return true;
}
bool propagate_type(ast_node_t *n, logic_expr_eq_t *, sematic_context_t *c) {
  return set_boolean_type(n, c);
}
bool propagate_type(ast_node_t *n, logic_expr_and_t *, sematic_context_t *c) {
  return set_boolean_type(n, c);
}
bool propagate_type(ast_node_t *n, bitor_expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
#define BIN(F, A, B)                                                           \
  bool F(ast_node_t *n, A *l, B *r, sematic_context_t *c) {                    \
    return binary(c, n, l, r);                                                 \
  }
BIN(check_addable, expr_t, term_t)
BIN(infer_binary_result_type, expr_t,
    term_t) BIN(check_minusable, expr_t, term_t) BIN(check_mulable, term_t,
                                                     power_factor_t)
    BIN(infer_binary_result_type, term_t,
        power_factor_t) BIN(check_divable, term_t, power_factor_t)
        BIN(check_modable, term_t,
            power_factor_t) BIN(check_bitandable, bitand_expr_t, shift_expr_t)
            BIN(infer_binary_result_type, bitand_expr_t,
                shift_expr_t) BIN(check_bitorable, bitor_expr_t, bitand_expr_t)
                BIN(infer_binary_result_type, bitor_expr_t, bitand_expr_t)
                    BIN(check_shiftleftable, shift_expr_t,
                        expr_t) BIN(infer_binary_result_type, shift_expr_t,
                                    expr_t)
                        BIN(check_shiftrightable, shift_expr_t,
                            expr_t) bool propagate_type(ast_node_t *n,
                                                        power_factor_t *e,
                                                        sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool require_numeric(ast_node_t *, power_factor_t *, sematic_context_t *) {
  return true;
}
bool require_numeric(ast_node_t *, single_op_factor_t *, sematic_context_t *) {
  return true;
}
bool infer_binary_result_type(ast_node_t *n, power_factor_t *l,
                              single_op_factor_t *r, sematic_context_t *c) {
  return binary(c, n, l, r);
}
bool propagate_type(ast_node_t *n, single_op_factor_t *e,
                    sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool require_lvalue(ast_node_t *, single_op_factor_t *, sematic_context_t *) {
  return true;
}
bool infer_pointer_type(ast_node_t *n, single_op_factor_t *,
                        sematic_context_t *c) {
  setty(c, n, "pointer");
  return true;
}
bool require_pointer(ast_node_t *, single_op_factor_t *, sematic_context_t *) {
  return true;
}
bool infer_pointee_type(ast_node_t *n, single_op_factor_t *,
                        sematic_context_t *c) {
  setty(c, n, "int");
  return true;
}
bool require_boolean(ast_node_t *, single_op_factor_t *, sematic_context_t *) {
  return true;
}
bool require_pointer(ast_node_t *, lvalue_t *, sematic_context_t *) {
  return true;
}
bool infer_pointee_type(ast_node_t *n, lvalue_t *, sematic_context_t *c) {
  setty(c, n, "int");
  return true;
}
bool require_lvalue(ast_node_t *, ultimate_expr_t *, sematic_context_t *) {
  return true;
}
bool propagate_type(ast_node_t *n, ultimate_expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_symbol_type(ast_node_t *n, token_t *t, sematic_context_t *c) {
  auto r = c ? c->lookup_symbol(V(t)) : symbol_ref_t{};
  if (!r.valid())
    return false;
  setty(c, n, S(c).symbols[r.symbol_id].type);
  return true;
}
bool propagate_type(ast_node_t *n, lvalue_noproperty_t *e,
                    sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool require_callable(ast_node_t *, lvalue_t *, sematic_context_t *) {
  return true;
}
bool check_argument_count(ast_node_t *, lvalue_t *, argument_list_t *,
                          sematic_context_t *) {
  return true;
}
bool check_argument_types(ast_node_t *, lvalue_t *, argument_list_t *,
                          sematic_context_t *) {
  return true;
}
bool infer_call_result_type(ast_node_t *n, lvalue_t *, sematic_context_t *c) {
  setty(c, n, "int");
  return true;
}
bool check_member_access(ast_node_t *, lvalue_t *, token_t *,
                         sematic_context_t *) {
  return true;
}
bool propagate_member_type(ast_node_t *n, lvalue_t *, token_t *,
                           sematic_context_t *c) {
  setty(c, n, "int");
  return true;
}
bool set_number_type(ast_node_t *n, token_t *, sematic_context_t *c) {
  setty(c, n, "int");
  return true;
}
bool set_char_type(ast_node_t *n, token_t *, sematic_context_t *c) {
  setty(c, n, "char");
  return true;
}
bool set_string_type(ast_node_t *n, token_t *, sematic_context_t *c) {
  setty(c, n, "string");
  return true;
}
bool propagate_type(ast_node_t *n, factor_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, term_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, shift_expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, bitand_expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, glued_factor_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool propagate_type(ast_node_t *n, assign_expr_t *e, sematic_context_t *c) {
  setty(c, n, TY(c, e));
  return true;
}
bool resolve_type_name(ast_node_t *, token_t *t, sematic_context_t *c) {
  auto v = V(t);
  return v == "int" || v == "char" || v == "string" || v == "void" ||
         v == "bool" || c->lookup_symbol(v).valid() ||
         bad(c, "unknown type: " + v);
}
bool set_named_type(ast_node_t *n, token_t *t, sematic_context_t *c) {
  setty(c, n, V(t));
  return true;
}
bool require_integer(ast_node_t *, ultimate_expr_t *, sematic_context_t *) {
  return true;
}
bool require_constant_expression(ast_node_t *, ultimate_expr_t *,
                                 sematic_context_t *) {
  return true;
}
bool require_positive(ast_node_t *, ultimate_expr_t *, sematic_context_t *) {
  return true;
}
bool require_function_context(ast_node_t *, sematic_context_t *c) {
  return c && !c->function_stack.empty();
}
bool check_empty_return_type(ast_node_t *, sematic_context_t *c) {
  if (!require_function_context(nullptr, c))
    return false;
  auto r = c->function_stack.back();
  return S(c).symbols[r.symbol_id].result == "void" ||
         bad(c, "non-void function requires a return value");
}
bool check_match_return_type(ast_node_t *, ultimate_expr_t *e,
                             sematic_context_t *c) {
  if (!require_function_context(nullptr, c))
    return false;
  auto r = c->function_stack.back();
  return compat(S(c).symbols[r.symbol_id].result, TY(c, e)) ||
         bad(c, "return type mismatch");
}
bool require_boolean(ast_node_t *, ultimate_expr_t *, sematic_context_t *) {
  return true;
}
bool begin_definite_function(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->begin_definite_function();
  return true;
}
bool mark_parameters_initialized(ast_node_t *, parameter_list_t *,
                                 sematic_context_t *c) {
  return c != nullptr;
}
bool end_definite_function(ast_node_t *, sematic_context_t *c) {
  return c && c->end_definite_function();
}
bool mark_initialized(ast_node_t *, token_t *t, sematic_context_t *c) {
  auto r = c ? c->lookup_symbol(V(t)) : symbol_ref_t{};
  if (!r.valid())
    return false;
  c->initialized.insert(r.symbol_id);
  return true;
}
bool mark_uninitialized(ast_node_t *, token_t *t, sematic_context_t *c) {
  auto r = c ? c->lookup_symbol(V(t)) : symbol_ref_t{};
  if (!r.valid())
    return false;
  c->initialized.erase(r.symbol_id);
  return true;
}
bool push_write_access(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->push_access_mode(sematic_access_mode_t::write);
  return true;
}
bool pop_access_mode(ast_node_t *, sematic_context_t *c) {
  return c && c->pop_access_mode();
}
bool mark_lvalue_initialized(ast_node_t *, lvalue_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool push_read_access(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->push_access_mode(sematic_access_mode_t::read);
  return true;
}
bool push_address_access(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->push_access_mode(sematic_access_mode_t::address);
  return true;
}
bool require_initialized_if_read(ast_node_t *, token_t *t,
                                 sematic_context_t *c) {
  if (!c || c->access_mode() != sematic_access_mode_t::read)
    return true;
  auto r = c->lookup_symbol(V(t));
  return r.valid() && c->initialized.count(r.symbol_id)
             ? true
             : bad(c, "read of uninitialized variable: " + V(t));
}
bool begin_optional_init(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->begin_optional_init();
  return true;
}
bool end_optional_init(ast_node_t *, sematic_context_t *c) {
  return c && c->end_optional_init();
}
bool begin_conditional_init(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->begin_conditional_init();
  return true;
}
bool begin_init_branch(ast_node_t *, sematic_context_t *c) {
  return c && c->begin_init_branch();
}
bool end_init_branch(ast_node_t *, sematic_context_t *c) {
  return c && c->end_init_branch();
}
bool end_conditional_init(ast_node_t *, sematic_context_t *c) {
  return c && c->end_conditional_init();
}
bool record_implicit_init_branch(ast_node_t *, sematic_context_t *c) {
  return c && c->record_implicit_init_branch();
}
bool save_init_condition_fallthrough(ast_node_t *, sematic_context_t *c) {
  return c && c->save_init_condition_fallthrough();
}
bool begin_loop_init(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  c->begin_loop_init();
  return true;
}
bool end_loop_init(ast_node_t *, sematic_context_t *c) {
  return c && c->end_loop_init();
}
bool require_all_paths_return(ast_node_t *, statements_t *,
                              sematic_context_t *c) {
  return c && S(c).flow == 1;
}
bool check_control_flow(ast_node_t *, ultimate_expr_t *, statements_t *,
                        elseif_else_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool check_control_flow(ast_node_t *, ultimate_expr_t *, statements_t *,
                        sematic_context_t *c) {
  return c != nullptr;
}
bool leave_loop_scope(ast_node_t *, sematic_context_t *c) {
  return c && c->leave_symbol_table();
}
bool require_loop_context(ast_node_t *, sematic_context_t *) { return true; }
bool set_terminating_flow(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  S(c).flow = 2;
  return true;
}
bool set_return_flow(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  S(c).flow = 1;
  return true;
}
bool set_fallthrough_flow(ast_node_t *, sematic_context_t *c) {
  if (!c)
    return false;
  S(c).flow = 0;
  return true;
}
bool propagate_flow(ast_node_t *, if_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool propagate_flow(ast_node_t *, while_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool propagate_flow(ast_node_t *, statement_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool check_unreachable(ast_node_t *, statement_t *, statements_nonempty_t *,
                       sematic_context_t *c) {
  return c != nullptr;
}
bool combine_sequential_flow(ast_node_t *, statement_t *,
                             statements_nonempty_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool propagate_flow(ast_node_t *, statements_nonempty_t *,
                    sematic_context_t *c) {
  return c != nullptr;
}
bool propagate_flow(ast_node_t *, statements_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool propagate_flow(ast_node_t *, else_t *, sematic_context_t *c) {
  return c != nullptr;
}
bool combine_branch_flow(ast_node_t *, elseif_t *, elseif_else_t *,
                         sematic_context_t *c) {
  return c != nullptr;
}
