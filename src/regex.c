#include "reg_malloc.h"
#include "reg_parse.h"
#include "reg_state.h"
#include "state_pattern.h"

#include "reg_error.h"
#include "reg_list.h"
#include "regex.h"
#include <string.h>

struct reg_env {
  struct reg_longjump *exception_chain;

  struct reg_parse *parse_p;
  struct reg_state *state_p;
};

struct reg_longjump **reg_get_exception(struct reg_env *env) {
  return &(env->exception_chain);
}

REG_API struct reg_env *reg_open_env() {
  struct reg_env *env = (struct reg_env *)malloc(sizeof(struct reg_env));
  env->exception_chain = NULL;
  env->parse_p = parse_new(env);
  env->state_p = state_new(env);
  return env;
}

REG_API void reg_close_env(struct reg_env *env) {
  parse_free(env->parse_p);
  state_free(env->state_p);
  free(env);
}

struct _pattern_arg {
  struct reg_env *env;
  const char *rule;
  struct reg_pattern *pattern;
};

static void _pgen_pattern(struct _pattern_arg *argv) {
  struct reg_ast_node *root =
      parse_exec(argv->env->parse_p, argv->rule, strlen(argv->rule));
  int is_match_tail = parse_is_match_tail(argv->env->parse_p);
  argv->pattern = state_new_pattern(argv->env->state_p, root, is_match_tail);
}

REG_API struct reg_pattern *reg_new_pattern(struct reg_env *env,
                                            const char *rule) {
  if (rule == NULL || env == NULL)
    return NULL;

  parse_clear(env->parse_p);

  // set exception handling
  struct _pattern_arg argv = {
      .env = env, .rule = rule, .pattern = NULL,
  };

  if (reg_cpcall(env, (pfunc)_pgen_pattern, &argv)) {
    return NULL;
  }

  return argv.pattern;
}

REG_API void reg_free_pattern(struct reg_pattern *pattern) {
  state_free_pattern(pattern);
}

REG_API int reg_match(struct reg_pattern *pattern, const char *source,
                      int len) {
  return state_match(pattern, source, len);
  //return state_match_opt(pattern, source, len);
}

REG_API struct fast_dfa_t *lvzixun_regex_get_fast_dfa(struct reg_env *env,
                                                const char *rule) {
  struct reg_pattern *pattern = reg_new_pattern(env, rule);
  struct fast_dfa_t *fast_dfa =
      (struct fast_dfa_t *)malloc(sizeof(struct fast_dfa_t));
  postprocess_dfa(pattern, fast_dfa);
  return fast_dfa;
}

REG_API int lvzixun_fast_dfa_reg_match(struct fast_dfa_t *fast_dfa, const char *source) {
  return lvzixun_fast_dfa_state_match(fast_dfa, source);
}

REG_API void lvzixun_fast_dfa_reg_match_batch(struct fast_dfa_t *fast_dfa, char *source[8], int ret[8]) {
  lvzixun_fast_dfa_state_match_batch(fast_dfa, source, ret);
}

REG_API void lvzixun_fast_dfa_reg_sum_batch(struct fast_dfa_t *fast_dfa, char *source[8], int ret[8]) {
  lvzixun_fast_dfa_state_sum_batch(fast_dfa, source, ret);
}
