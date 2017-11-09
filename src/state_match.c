/*
  match regex rule from minimization dfa
*/

#include <stdio.h>
#include "state_pattern.h"
#include "reg_stream.h"
#include "reg_list.h"
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "regex.h"

static int _match_dfa_state(struct reg_pattern* pattern, size_t node_pos, struct reg_stream* source);
static int _match_nfa_state(struct reg_pattern* pattern, size_t node_pos, struct reg_stream* source);
struct timespec start, end;

int state_match(struct reg_pattern* pattern, const char* s, int len){
  assert(_match_nfa_state); // filter warning

  struct reg_stream* source = stream_new((const unsigned char*)s, len);
  int success = _match_dfa_state(pattern, pattern->min_dfa_start_state_pos, source);
    
  stream_free(source);
  return success;
}


static int _match_dfa_state(struct reg_pattern* pattern, size_t node_pos, struct reg_stream* source){
  size_t state_list_len = list_len(pattern->state_list);
  for (size_t i = 1; i < state_list_len; i++) {
    struct reg_node* node = state_node_pos(pattern, i);
    assert(node != NULL);
  }

  //printf("_match_dfa_state: node_pos = %zu, state list size = %zu\n", node_pos, list_len(pattern->state_list));

#define MAX_NUM_STATES 255
  const size_t initial_dfa_node_pos = node_pos;

  // seen[i] == 1 means that I have "seen" state i. It does not mean that
  // I have "visited" state i (in the traditional BFS visited sense).
  uint8_t seen[MAX_NUM_STATES] = {0};
  uint8_t visited[MAX_NUM_STATES] = {0};
  uint8_t *states[MAX_NUM_STATES] = {NULL};

  // bool_new_visited becomes false when I don't visit any new states in a pass
  uint8_t bool_new_visited = 1;
  seen[initial_dfa_node_pos] = 1;

  while (bool_new_visited == 1) {
    bool_new_visited = 0;  // Assume that we won't "see" any new states in this pass

    for (size_t state_i = 0; state_i < 100; state_i++) {
      if (seen[state_i] == 0) continue;
      if (visited[state_i] == 1) continue;

      struct reg_node* node = state_node_pos(pattern, state_i);
      assert(node != NULL);
      struct reg_list* edges = node->edges;
      struct _reg_path* path = NULL;

      // This state is seen but not visited. Now visiting state_i.
      //printf("Visiting state %zu.\n", state_i);

      assert(states[state_i] == NULL);
      states[state_i] = malloc(256 * sizeof(uint8_t));
      for (size_t edge_i = 0; edge_i < 256; edge_i++) {
        assert(initial_dfa_node_pos <= MAX_NUM_STATES);
        states[state_i][edge_i] = (uint8_t)node_pos;
      }

      for (size_t edge_i = 0; (path = list_idx(edges, edge_i)); edge_i++) {
        size_t next_node_pos = path->next_node_pos;
        assert(next_node_pos < MAX_NUM_STATES);

        struct reg_range* range = &(state_edge_pos(pattern, path->edge_pos)->range);
        assert(range != NULL);

        for (uint8_t c = (char)(range->begin); c <= (char)(range->end); c++) {
          assert(states[state_i][c] == initial_dfa_node_pos);
          states[state_i][c] = (uint8_t)next_node_pos;
          //printf("Adding edge state[%zu][%u] to state %zu.\n", state_i, c, next_node_pos);
        }

        if (seen[next_node_pos] == 0) {
          //printf("Visiting new state %zu from node %zu.\n", next_node_pos, state_i);
          // Record that we "saw" a new state in this pass, so that we do
          // another pass. The next pass will "visit" this state (in the
          // traditional BFS visiting sense).
          bool_new_visited = 1;
          seen[next_node_pos] = 1;
        }
      }

      visited[state_i] = 1;
    }
  }

  int t = ((rand() & 65535) == 65535);
  if (t) { clock_gettime(CLOCK_REALTIME, &start);}
  for(; !stream_end(source);) {
    unsigned char c = stream_char(source);
    struct reg_node* node = state_node_pos(pattern, node_pos);
    if (node->is_end && !pattern->is_match_tail) return 1;
    if (!states[node_pos]) return 0;
    size_t next_node_pos = states[node_pos][c];
    node_pos = next_node_pos;
    stream_next(source);
  }
  if (t) {
    clock_gettime(CLOCK_REALTIME, &end);
    size_t tot_ns = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
    printf("regex match Overhead: Time per measurement = %.2f ns\n", (double)tot_ns);
  }
  return state_node_pos(pattern, node_pos)->is_end;

  /*
  for(;!stream_end(source);){
    printf("node_pos = %zu.\n", node_pos);
    // dump edge
    struct reg_node* node = state_node_pos(pattern, node_pos);
    struct reg_list* edges = node->edges;
    struct _reg_path* path = NULL;
    unsigned char c = stream_char(source);    

    if(node->is_end && !pattern->is_match_tail) return 1;

    for(size_t i=0; (path = list_idx(edges, i)); i++){
      struct reg_range* range = &(state_edge_pos(pattern, path->edge_pos)->range);
      size_t next_node_pos = path->next_node_pos;

      assert(range);
      printf("range: %c, %c. next node = %zu.\n", (char)(range->begin), (char)(range->end), next_node_pos);
      if(c >= range->begin && c<=range->end){ // range
        node_pos = next_node_pos;
        stream_next(source);
        break;
      }
    }

    if(!path) return 0;
  }

  // match is end of source
  return state_node_pos(pattern, node_pos)->is_end;
  */
}


static int _match_nfa_state(struct reg_pattern* pattern, size_t node_pos, struct reg_stream* source){
  // pass end state
  if(stream_end(source) && state_node_pos(pattern, node_pos)->is_end){ 
    return 1;
  }

  // dump edge
  struct reg_list* edges = state_node_pos(pattern, node_pos)->edges;
  struct _reg_path* path = NULL;
  unsigned char c = stream_char(source);


  int success = 0;
  for(size_t i=0; (path = list_idx(edges, i)); i++){
    struct reg_range* range = &(state_edge_pos(pattern, path->edge_pos)->range);
    size_t next_node_pos = path->next_node_pos;

    if(range == NULL){  //edsilone
      success = _match_nfa_state(pattern, next_node_pos, source);
    }else if(c >= range->begin && c<=range->end){ // range
      if(stream_next(source)){
        assert(success==0);
        success = _match_nfa_state(pattern, next_node_pos, source);
        stream_back(source);
      }
    }

    if(success) return 1; 
  }

  return 0;
}


