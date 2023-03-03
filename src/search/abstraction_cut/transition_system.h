#ifndef ABSTRACTION_CUT_TRANSITION_SYSTEM_H
#define ABSTRACTION_CUT_TRANSITION_SYSTEM_H

#include "../task_proxy.h"

#include "../lp/lp_solver.h"

#include <set>
#include <vector>

namespace abstraction_cut {
struct Transition {
    int id;
    int src;
    int label;
    int dst;
    bool is_zero_cost; // TODO: Cut 1 bit from label to store this

    Transition(int id, int src, int label, int dst, bool is_zero_cost)
        : id(id),
          src(src),
          label(label),
          dst(dst),
          is_zero_cost(is_zero_cost) {}

    friend std::ostream &operator<<(std::ostream &os, const Transition &t) {
        return os << "(" << t.src << "," << t.label << "," << t.dst << (t.is_zero_cost? ", 0-cost)" : ")");
    }
};

struct TransitionSystem {
    explicit TransitionSystem(int _num_states, int _num_labels, std::vector<Transition> &&_transitions, std::vector<int> &&_goal_states);
    // IDs of states are assumed to go from 0 to num_states - 1.
    int num_states;
    // IDs of labels are assumed to go from 0 to num_labels - 1.
    int num_labels;

    std::vector<Transition> transitions;
    std::vector<int> goal_states;
    
    std::vector<std::shared_ptr<Transition>> backward_transitions;
    std::vector<int> backward_offsets;
    std::vector<std::shared_ptr<Transition>> forward_transitions;
    std::vector<int> forward_offsets;
};

std::vector<bool> compute_alive_states(const TransitionSystem &ts,
                                       int initial_state);
std::vector<bool> compute_alive_labels(const TransitionSystem &ts,
                                       const std::vector<bool> &state_alive);

/*
  Computes a new transition system that is an abstraction of the old one,
  mapping state s to state new_state_ids[s] and label l to label new_label_ids[l].
  States and labels that map to -1 are removed.
*/
TransitionSystem prune_transition_system(
        const TransitionSystem &ts, const std::vector<int> &new_state_ids,
        const std::vector<int> &new_label_ids);

std::set<int> get_nonzero_cost_incoming_transitions(
    const TransitionSystem &ts, int abstract_state_id,
    const std::vector<bool> &exclude_state);
std::set<int> get_nonzero_cost_outgoing_transitions(
    const TransitionSystem &ts, int abstract_state_id,
    const std::vector<bool> &exclude_state);

void get_zero_cost_predecessors(
        const TransitionSystem &ts, int abstract_state_id,
        const std::vector<bool> &exclude_state, std::set<int> &predecessors);
void get_zero_cost_successors(
    const TransitionSystem &ts, int abstract_state_id,
    const std::vector<bool> &exclude_state, std::set<int> &successors);

void dump(const TransitionSystem &ts);
}

#endif
