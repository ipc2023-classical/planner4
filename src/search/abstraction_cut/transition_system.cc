#include "transition_system.h"

#include "../utils/logging.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <numeric>
#include <queue>

using namespace std;

namespace abstraction_cut {
TransitionSystem::TransitionSystem(int _num_states, int _num_labels, vector<Transition> &&_transitions, vector<int> &&_goal_states)
        : num_states(_num_states), num_labels(_num_labels), transitions(move(_transitions)), goal_states(move(_goal_states)) {
    int transition_id = 0;
    for (Transition &t : transitions) {
        assert(t.id == -1);
        t.id = transition_id;
        ++transition_id;
        backward_transitions.push_back(make_shared<Transition>(t));
        forward_transitions.push_back(make_shared<Transition>(t));
    }

    // Create forward representation of transition system
    sort(backward_transitions.begin(), backward_transitions.end(),
         [] (const shared_ptr<Transition> lhs, const shared_ptr<Transition> rhs) {
             if (lhs->dst == rhs->dst) {
                 if (lhs->is_zero_cost == rhs->is_zero_cost) {
                     return lhs->src < rhs->src;
                 }
                 return lhs->is_zero_cost;
             }
             return lhs->dst < rhs->dst;
         });

    backward_offsets.reserve(num_states + 1);
    backward_offsets.push_back(0);
    int transition_index = 0;
    for (int state_id = 0; state_id < num_states; ++state_id) {
        while ((transition_index < static_cast<int>(transitions.size())) &&
               (backward_transitions[transition_index]->dst == state_id)) {
            ++transition_index;
        }
        backward_offsets.push_back(transition_index);
    }

    // Create forward representation of transition system
    sort(forward_transitions.begin(), forward_transitions.end(),
         [] (const shared_ptr<Transition> lhs, const shared_ptr<Transition> rhs) {
             if (lhs->src == rhs->src) {
                 if (lhs->is_zero_cost == rhs->is_zero_cost) {
                     return lhs->dst < rhs->dst;
                 }
                 return lhs->is_zero_cost;
             }
             return lhs->src < rhs->src;
         });

    forward_offsets.reserve(num_states + 1);
    forward_offsets.push_back(0);
    transition_index = 0;
    for (int state_id = 0; state_id < num_states; ++state_id) {
        while ((transition_index < static_cast<int>(forward_transitions.size())) &&
               (forward_transitions[transition_index]->src == state_id)) {
            ++transition_index;
        }
        forward_offsets.push_back(transition_index);
    }
}

static vector<bool> compute_reachability(const vector<int> &initial_states, const vector<vector<int>> &successors) {
    vector<bool> reachable(successors.size(), false);
    deque<int> queue;
    for (int s : initial_states) {
        queue.push_back(s);
        reachable[s] = true;
    }
    while (!queue.empty()) {
        int s = queue.front();
        queue.pop_front();
        for (int succ : successors[s]) {
            if (!reachable[succ]) {
                queue.push_back(succ);
                reachable[succ] = true;
            }
        }
    }
    return reachable;
}

vector<bool> compute_alive_states(const TransitionSystem &ts, int initial_state) {
    int num_states = ts.num_states;
    vector<vector<int>> forward_successors(num_states);
    vector<vector<int>> backward_successors(num_states);
    for (shared_ptr<Transition> t : ts.backward_transitions) {
        forward_successors[t->src].push_back(t->dst);
        backward_successors[t->dst].push_back(t->src);
    }
    for (int i = 0; i < num_states; ++i) {
        utils::sort_unique(forward_successors[i]);
        utils::sort_unique(backward_successors[i]);
    }

    vector<bool> forward_reachable = compute_reachability({initial_state}, forward_successors);
    vector<bool> backward_reachable = compute_reachability(ts.goal_states, backward_successors);
    vector<bool> state_alive(num_states);
    for (int s = 0; s < num_states; ++s) {
        state_alive[s] = forward_reachable[s] && backward_reachable[s];
    }
    return state_alive;
}

vector<bool> compute_alive_labels(const TransitionSystem &ts, const vector<bool> &state_alive) {
    vector<bool> label_alive(ts.num_labels, false);
    for (shared_ptr<Transition> t : ts.backward_transitions) {
        if (state_alive[t->src] && state_alive[t->dst]) {
            label_alive[t->label] = true;
        }
    }
    return label_alive;
}

TransitionSystem prune_transition_system(const TransitionSystem &ts,
                                         const vector<int> &new_state_ids,
                                         const vector<int> &new_label_ids) {
    vector<int> new_goal_states;
    new_goal_states.reserve(ts.goal_states.size());
    for (int s : ts.goal_states) {
        if (new_state_ids[s] >= 0) {
            new_goal_states.push_back(new_state_ids[s]);
        }
    }
    assert(!new_goal_states.empty());

    vector<Transition> new_transitions;
    new_transitions.reserve(ts.transitions.size());
    for (const Transition &t : ts.transitions) {
        if (new_state_ids[t.src] >= 0
            && new_state_ids[t.dst] >= 0
            && new_label_ids[t.label] >= 0) {
            new_transitions.push_back({
                    -1,
                    new_state_ids[t.src],
                    new_label_ids[t.label],
                    new_state_ids[t.dst],
                    t.is_zero_cost
                });
        }
    }

    int max_state = new_state_ids.empty()
        ? -1
        : *max_element(new_state_ids.begin(), new_state_ids.end());
    int max_label = new_label_ids.empty()
        ? -1
        : *max_element(new_label_ids.begin(), new_label_ids.end());
    // Pruned labels are grouped under a new label ID (without any transitions).
    // TODO: I don't think we need to keep pruned labels
    if (find(new_label_ids.begin(), new_label_ids.end(), -1) != new_label_ids.end()) {
        ++max_label;
    }

    return TransitionSystem(max_state + 1, max_label + 1, move(new_transitions), move(new_goal_states));
}

set<int> get_nonzero_cost_incoming_transitions(
    const TransitionSystem &ts, int abstract_state_id, const vector<bool> &exclude_state) {
    assert(utils::in_bounds(abstract_state_id, ts.backward_offsets));
    assert(utils::in_bounds(abstract_state_id+1, ts.backward_offsets));

    set<int> transition_ids;
    for (int index = ts.backward_offsets[abstract_state_id]; index < ts.backward_offsets[abstract_state_id+1]; ++index) {
        shared_ptr<Transition> t = ts.backward_transitions[index];
        assert(t->dst == abstract_state_id);
        if (t->is_zero_cost || exclude_state[t->src]) {
            continue;
        }
        transition_ids.insert(t->id);
    }
    return transition_ids;
}

set<int> get_nonzero_cost_outgoing_transitions(
    const TransitionSystem &ts, int abstract_state_id, const vector<bool> &exclude_state) {
    assert(utils::in_bounds(abstract_state_id, ts.forward_offsets));
    assert(utils::in_bounds(abstract_state_id+1, ts.forward_offsets));

    set<int> transition_ids;
    for (int index = ts.forward_offsets[abstract_state_id]; index < ts.forward_offsets[abstract_state_id+1]; ++index) {
        shared_ptr<Transition> t = ts.forward_transitions[index];
        assert(t->src == abstract_state_id);
        if (t->is_zero_cost || exclude_state[t->dst]) {
            continue;
        }
        transition_ids.insert(t->id);
    }
    return transition_ids;
}

void get_zero_cost_predecessors(
    const TransitionSystem &ts, int abstract_state_id,
    const vector<bool> &exclude_state, set<int> &predecessors) {
    for (int index = ts.backward_offsets[abstract_state_id]; index < ts.backward_offsets[abstract_state_id+1]; ++index) {
        shared_ptr<Transition> t = ts.backward_transitions[index];
        assert(t->dst == abstract_state_id);
        if (!t->is_zero_cost) {
            return;
        }
        if (!exclude_state[t->src]) {
            predecessors.insert(t->src);
        }
    }
}

void get_zero_cost_successors(
    const TransitionSystem &ts, int abstract_state_id,
    const vector<bool> &exclude_state, set<int> &successors) {
    for (int index = ts.forward_offsets[abstract_state_id]; index < ts.forward_offsets[abstract_state_id+1]; ++index) {
        shared_ptr<Transition> t = ts.forward_transitions[index];
        assert(t->src == abstract_state_id);
        if (!t->is_zero_cost) {
            return;
        }
        if (!exclude_state[t->dst]) {
            successors.insert(t->dst);
        }
    }
}

void dump(const TransitionSystem &ts) {
    cout << "Transition system with " << ts.num_states << " states and "
         << ts.num_labels << " labels:" << endl;
    cout << "  Goal states: " << ts.goal_states << endl;
    // cout << "  Backward Offsets: " << ts.backward_offsets << endl;
    // cout << "  Backward Transitions:" << endl;
    // for (shared_ptr<Transition> t : ts.backward_transitions) {
    //     cout << "    " << t->id << ": " << t->src << " --" << t->label << "--> " << t->dst <<  (t->is_zero_cost? ", 0-cost)" : "") << endl;
    // }
    cout << "  Forward Offsets: " << ts.forward_offsets << endl;
    cout << "  Forward Transitions:" << endl;
    for (shared_ptr<Transition> t : ts.forward_transitions) {
        cout << "    " << t->id << ": " << t->src << " --{" << t->label << "}--> " << t->dst <<  (t->is_zero_cost? ", 0-cost)" : "") << endl;
    }
}
}
