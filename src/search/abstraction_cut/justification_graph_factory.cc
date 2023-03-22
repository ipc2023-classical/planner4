#include "justification_graph_factory.h"

#include "../task_utils/task_properties.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <map>
#include <string>

using namespace std;

namespace abstraction_cut {
// construction and destruction
JustificationGraphFactory::JustificationGraphFactory(
    const TaskProxy &task_proxy) {
    task_properties::verify_no_axioms(task_proxy);
    task_properties::verify_no_conditional_effects(task_proxy);

    // Build propositions.
    num_propositions = 2; // artificial goal and artificial precondition
    VariablesProxy variables = task_proxy.get_variables();
    propositions.resize(variables.size());
    for (FactProxy fact : variables.get_facts()) {
        int var_id = fact.get_variable().get_id();
        propositions[var_id].push_back(RelaxedProposition());
        ++num_propositions;
        prop_to_fact.emplace(make_pair(var_id, propositions[var_id].size() - 1), fact);
        cout << "added fact to prop to fact: " << fact.get_name() << endl;
    }

    // Build relaxed operators for operators and axioms.
    for (OperatorProxy op : task_proxy.get_operators()) {
        build_relaxed_operator(op);
        op_id_to_op.emplace(op.get_id(), op);
    }

    // Simplify relaxed operators.
    // simplify();
    /* TODO: Put this back in and test if it makes sense,
       but only after trying out whether and how much the change to
       unary operators hurts. */

    // Build artificial goal proposition and operator.
    vector<RelaxedProposition *> goal_op_pre, goal_op_eff;
    for (FactProxy goal : task_proxy.get_goals()) {
        goal_op_pre.push_back(get_proposition(goal));
    }
    goal_op_eff.push_back(&artificial_goal);
    /* Use the invalid operator ID -1 so accessing
       the artificial operator will generate an error. */
    add_relaxed_operator(move(goal_op_pre), move(goal_op_eff), -1, 0);

    // Cross-reference relaxed operators.
    for (RelaxedOperator &op : relaxed_operators) {
        for (RelaxedProposition *pre : op.preconditions)
            pre->precondition_of.push_back(&op);
        for (RelaxedProposition *eff : op.effects)
            eff->effect_of.push_back(&op);
    }
}

JustificationGraphFactory::~JustificationGraphFactory() {
}

void JustificationGraphFactory::build_relaxed_operator(const OperatorProxy &op) {
    vector<RelaxedProposition *> precondition;
    vector<RelaxedProposition *> effects;
    for (FactProxy pre : op.get_preconditions()) {
        precondition.push_back(get_proposition(pre));
    }
    for (EffectProxy eff : op.get_effects()) {
        effects.push_back(get_proposition(eff.get_fact()));
    }
    add_relaxed_operator(
        move(precondition), move(effects), op.get_id(), op.get_cost());
}

void JustificationGraphFactory::add_relaxed_operator(
    vector<RelaxedProposition *> &&precondition,
    vector<RelaxedProposition *> &&effects,
    int op_id, int base_cost) {
    RelaxedOperator relaxed_op(
        move(precondition), move(effects), op_id, base_cost);
    if (relaxed_op.preconditions.empty())
        relaxed_op.preconditions.push_back(&artificial_precondition);
    relaxed_operators.push_back(relaxed_op);
}

RelaxedProposition *JustificationGraphFactory::get_proposition(
    const FactProxy &fact) {
    int var_id = fact.get_variable().get_id();
    int val = fact.get_value();
    return &propositions[var_id][val];
}

// TODO: Remove this function.
string JustificationGraphFactory::get_fact_string(RelaxedProposition *prop) {
    if (prop == &artificial_goal) {
        return "artificial goal";
    }
    if (prop == &artificial_precondition) {
        return "artificial precondition";
    }
    for (size_t i = 0; i < propositions.size(); ++i) {
        for (size_t j = 0; j < propositions[i].size(); ++j) {
            if (&propositions[i][j] == prop) {
                return prop_to_fact.at(make_pair(i, j)).get_name();
            }
        }
    }
    //return prop_to_fact.at(prop).get_name();
}

// heuristic computation
void JustificationGraphFactory::setup_exploration_queue() {
    priority_queue.clear();

    for (auto &var_props : propositions) {
        for (RelaxedProposition &prop : var_props) {
            prop.status = UNREACHED;
        }
    }

    artificial_goal.status = UNREACHED;
    artificial_precondition.status = UNREACHED;

    for (RelaxedOperator &op : relaxed_operators) {
        op.unsatisfied_preconditions = op.preconditions.size();
        op.h_max_supporter = 0;
        op.h_max_supporter_cost = numeric_limits<int>::max();
    }
}

void JustificationGraphFactory::setup_exploration_queue_state(const State &state) {
    for (FactProxy init_fact : state) {
        enqueue_if_necessary(get_proposition(init_fact), 0);
    }
    enqueue_if_necessary(&artificial_precondition, 0);
}

void JustificationGraphFactory::first_exploration(const State &state) {
    assert(priority_queue.empty());
    setup_exploration_queue();
    setup_exploration_queue_state(state);
    while (!priority_queue.empty()) {
        pair<int, RelaxedProposition *> top_pair = priority_queue.pop();
        int popped_cost = top_pair.first;
        RelaxedProposition *prop = top_pair.second;
        int prop_cost = prop->h_max_cost;
        assert(prop_cost <= popped_cost);
        if (prop_cost < popped_cost)
            continue;
        const vector<RelaxedOperator *> &triggered_operators =
            prop->precondition_of;
        for (RelaxedOperator *relaxed_op : triggered_operators) {
            --relaxed_op->unsatisfied_preconditions;
            assert(relaxed_op->unsatisfied_preconditions >= 0);
            if (relaxed_op->unsatisfied_preconditions == 0) {
                relaxed_op->h_max_supporter = prop;
                relaxed_op->h_max_supporter_cost = prop_cost;
                int target_cost = prop_cost + relaxed_op->cost;
                for (RelaxedProposition *effect : relaxed_op->effects) {
                    enqueue_if_necessary(effect, target_cost);
                }
            }
        }
    }
}

void JustificationGraphFactory::first_exploration_incremental(
    vector<RelaxedOperator *> &cut) {
    assert(priority_queue.empty());
    /* We pretend that this queue has had as many pushes already as we
       have propositions to avoid switching from bucket-based to
       heap-based too aggressively. This should prevent ever switching
       to heap-based in problems where action costs are at most 1.
    */
    priority_queue.add_virtual_pushes(num_propositions);
    for (RelaxedOperator *relaxed_op : cut) {
        int cost = relaxed_op->h_max_supporter_cost + relaxed_op->cost;
        for (RelaxedProposition *effect : relaxed_op->effects)
            enqueue_if_necessary(effect, cost);
    }
    while (!priority_queue.empty()) {
        pair<int, RelaxedProposition *> top_pair = priority_queue.pop();
        int popped_cost = top_pair.first;
        RelaxedProposition *prop = top_pair.second;
        int prop_cost = prop->h_max_cost;
        assert(prop_cost <= popped_cost);
        if (prop_cost < popped_cost)
            continue;
        const vector<RelaxedOperator *> &triggered_operators =
            prop->precondition_of;
        for (RelaxedOperator *relaxed_op : triggered_operators) {
            if (relaxed_op->h_max_supporter == prop) {
                int old_supp_cost = relaxed_op->h_max_supporter_cost;
                if (old_supp_cost > prop_cost) {
                    relaxed_op->update_h_max_supporter();
                    int new_supp_cost = relaxed_op->h_max_supporter_cost;
                    if (new_supp_cost != old_supp_cost) {
                        // This operator has become cheaper.
                        assert(new_supp_cost < old_supp_cost);
                        int target_cost = new_supp_cost + relaxed_op->cost;
                        for (RelaxedProposition *effect : relaxed_op->effects)
                            enqueue_if_necessary(effect, target_cost);
                    }
                }
            }
        }
    }
}

void JustificationGraphFactory::second_exploration(
    const State &state, vector<RelaxedProposition *> &second_exploration_queue,
    vector<RelaxedOperator *> &cut) {
    assert(second_exploration_queue.empty());
    assert(cut.empty());

    artificial_precondition.status = BEFORE_GOAL_ZONE;
    second_exploration_queue.push_back(&artificial_precondition);

    for (FactProxy init_fact : state) {
        RelaxedProposition *init_prop = get_proposition(init_fact);
        init_prop->status = BEFORE_GOAL_ZONE;
        second_exploration_queue.push_back(init_prop);
    }

    while (!second_exploration_queue.empty()) {
        RelaxedProposition *prop = second_exploration_queue.back();
        second_exploration_queue.pop_back();
        const vector<RelaxedOperator *> &triggered_operators =
            prop->precondition_of;
        for (RelaxedOperator *relaxed_op : triggered_operators) {
            if (relaxed_op->h_max_supporter == prop) {
                bool reached_goal_zone = false;
                for (RelaxedProposition *effect : relaxed_op->effects) {
                    if (effect->status == GOAL_ZONE) {
                        assert(relaxed_op->cost > 0);
                        reached_goal_zone = true;
                        cut.push_back(relaxed_op);
                        break;
                    }
                }
                if (!reached_goal_zone) {
                    for (RelaxedProposition *effect : relaxed_op->effects) {
                        if (effect->status != BEFORE_GOAL_ZONE) {
                            assert(effect->status == REACHED);
                            effect->status = BEFORE_GOAL_ZONE;
                            second_exploration_queue.push_back(effect);
                        }
                    }
                }
            }
        }
    }
}


pair<TransitionSystem, vector<vector<int>>>
JustificationGraphFactory::build_justification_graph(const State &state) {
    // TODO: Implement.
    // The second_exploration does first in, last out, but I think we want first
    // in, first out. That why it's a queue instead of a vector.
    // The `second` int is the proposition's "abstract_state_id"
    queue<pair<RelaxedProposition *, int>> exploration_queue;
    //vector<vector<int>> label_mapping;
    int num_states = 0;
    int num_labels = 0;
    // Artificial transitions get the label -1.
    vector<Transition> transitions;
    vector<int> goal_states{-1};
    // Transition(int id, int src, int label, int dst, bool is_zero_cost)
    // closed maps from propositions to their "abstract_state_id"
    map<RelaxedProposition *, int> closed;
    // Map every operator that appears in the transition system to a label.
    // We can't use label IDs directly because label IDs have to go from 0 to
    // num_labels-1.
    map<int, int> op_id_to_label;
    // Handle the artificial_precondition -> init_fact transitions.
    // This should also be the mapping for the goal operator.
    op_id_to_label.emplace(-1, 0);
    ++num_labels;

    exploration_queue.emplace(&artificial_precondition, num_states++);
    cout << "id " << num_states-1 << ": " << get_fact_string(&artificial_precondition) << endl;
    int artificial_init_id = exploration_queue.back().second;

    for (FactProxy init_fact : state) {
        RelaxedProposition *init_prop = get_proposition(init_fact);
        exploration_queue.emplace(init_prop, num_states++);
        cout << "id " << num_states-1 << ": " << get_fact_string(init_prop) << endl;
        transitions.emplace_back(-1, artificial_init_id, 0,
                                 exploration_queue.back().second, true);
    }
    for (const auto &vec : propositions) {
        for (const auto &element : vec) {
            //cout << element.status << endl;
        }
    }

    while (!exploration_queue.empty()) {
        pair<RelaxedProposition *, int> popped = exploration_queue.front();
        closed.insert(popped);
        exploration_queue.pop();
        const vector<RelaxedOperator *> &triggered_operators =
            popped.first->precondition_of;
        for (RelaxedOperator *relaxed_op : triggered_operators) {
            // TODO: Remove debug print thing.
            for (RelaxedProposition *effect : relaxed_op->effects) {
                if (effect == &artificial_goal) {
                    cout << "goal op h max supporter: " << get_fact_string(relaxed_op->h_max_supporter) << endl;
                }
            }
            if (relaxed_op->h_max_supporter != popped.first) {
                continue;
            }
            for (RelaxedProposition *effect : relaxed_op->effects) {
                if (effect == &artificial_goal) {
                    cout << "artificial goal is being achieved" << endl;
                }
                int effect_prop_id;
                // Get its "abstract_state_id" if the proposition is in the
                // closed list, insert it otherwise.
                if (closed.count(effect) > 0) {
                    effect_prop_id = closed.at(effect);
                } else {
                    exploration_queue.emplace(effect, num_states++);
                    cout << "id " << num_states-1 << ": " << get_fact_string(effect) << endl;
                    effect_prop_id = exploration_queue.back().second;
                }
                // Add the transition from precondition to effect prop.
                bool is_zero_cost =
                    relaxed_op->base_cost == 0 ? true : false;
                if (op_id_to_label.count(relaxed_op->original_op_id) == 0) {
                    op_id_to_label.emplace(relaxed_op->original_op_id, num_labels++);
                }
                transitions.emplace_back(-1, popped.second,
                                         op_id_to_label.at(relaxed_op->original_op_id),
                                         effect_prop_id, is_zero_cost);
                // Check if is goal.
                if (goal_states[0] == -1 && effect == &artificial_goal) {
                    goal_states[0] = effect_prop_id;
                }
            }
        }
    }
    cout << "num_labels: " << num_labels << endl;
    vector<vector<int>> label_mapping(num_labels, vector<int>(1));
    for (const auto &element : op_id_to_label) {
        label_mapping[element.second] = vector<int>({element.first});
        if (element.first == -1) {
            continue;
        }
        cout << "op_id " << element.first  << ": " << op_id_to_op.at(element.first).get_name() << endl;
    }
    for (size_t i = 0; i < label_mapping.size(); ++i) {
        cout << "label_id " << i << " -> op_id ";
        for (const auto &element : label_mapping[i]) {
            cout << element << endl;
        }
    }

    TransitionSystem transition_system(num_states, num_labels, move(transitions), move(goal_states));
    dump(transition_system);
    return make_pair(transition_system, label_mapping);
}

void JustificationGraphFactory::mark_goal_plateau(RelaxedProposition *subgoal) {
    // NOTE: subgoal can be null if we got here via recursion through
    // a zero-cost action that is relaxed unreachable. (This can only
    // happen in domains which have zero-cost actions to start with.)
    // For example, this happens in pegsol-strips #01.
    if (subgoal && subgoal->status != GOAL_ZONE) {
        subgoal->status = GOAL_ZONE;
        for (RelaxedOperator *achiever : subgoal->effect_of)
            if (achiever->cost == 0)
                mark_goal_plateau(achiever->h_max_supporter);
    }
}


void JustificationGraphFactory::get_justification_graph(
    const State &state, vector<TransitionSystem> &transition_systems,
    vector<vector<vector<int>>> &label_mappings) {

    for (RelaxedOperator &op : relaxed_operators) {
        op.cost = op.base_cost;
    }
    // The following three variables could be declared inside the loop
    // ("second_exploration_queue" even inside second_exploration),
    // but having them here saves reallocations and hence provides a
    // measurable speed boost.
    vector<RelaxedOperator *> cut;
    Landmark landmark;
    vector<RelaxedProposition *> second_exploration_queue;
    first_exploration(state);
    if (artificial_goal.status == UNREACHED)
        // This leaves the output vectors empty.
        return;

    while (artificial_goal.h_max_cost != 0) {
        pair<TransitionSystem, vector<vector<int>>> justification_graph = build_justification_graph(state);
        transition_systems.push_back(justification_graph.first);
        label_mappings.push_back(justification_graph.second);


        mark_goal_plateau(&artificial_goal);
        assert(cut.empty());

        second_exploration(state, second_exploration_queue, cut);
        assert(!cut.empty());
        int cut_cost = numeric_limits<int>::max();
        for (RelaxedOperator *op : cut)
            cut_cost = min(cut_cost, op->cost);
        for (RelaxedOperator *op : cut)
            op->cost -= cut_cost;

        first_exploration_incremental(cut);
        // validate_h_max();  // too expensive to use even in regular debug mode
        cut.clear();

        /*
          Note: This could perhaps be made more efficient, for example by
          using a round-dependent counter for GOAL_ZONE and BEFORE_GOAL_ZONE,
          or something based on total_cost, so that we don't need a per-round
          reinitialization.
        */
        for (auto &var_props : propositions) {
            for (RelaxedProposition &prop : var_props) {
                if (prop.status == GOAL_ZONE || prop.status == BEFORE_GOAL_ZONE)
                    prop.status = REACHED;
            }
        }
        artificial_goal.status = REACHED;
        artificial_precondition.status = REACHED;
    }
    // TODO: Make sure the transition systems and label mappings were added to
    // their respective vectors.
    return;
}
}
