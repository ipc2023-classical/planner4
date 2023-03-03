#include "projections.h"

#include "../plugins/options.h"
#include "../task_utils/task_properties.h"
#include "../tasks/projected_task.h"
#include "../utils/math.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <limits>

using namespace std;

namespace abstraction_cut {
AbstractionFunction::AbstractionFunction(const pdbs::Pattern &pattern,
                                         const vector<int> &hash_multipliers,
                                         vector<vector<int>> &&inverse_label_mapping)
    : pattern(pattern),
      hash_multipliers(hash_multipliers),
      inverse_label_mapping(move(inverse_label_mapping)) {
}

AbstractionFunction::AbstractionFunction(
    AbstractionFunction &&other,
    const vector<int> &new_state_ids,
    const vector<int> &new_label_ids)
    : pattern(other.pattern),
      hash_multipliers(other.hash_multipliers),
      state_mapping(new_state_ids.size(), -1) {
    int num_states = state_mapping.size();
    for (int s = 0; s < num_states; ++s) {
        if (other.state_mapping.empty()) {
            state_mapping[s] = new_state_ids[s];
        } else {
            state_mapping[s] = new_state_ids[other.state_mapping[s]];
        }
    }

    int max_label = new_label_ids.empty()
        ? -1
        : *max_element(new_label_ids.begin(), new_label_ids.end());
    // Reserve one extra slot for possible pruned labels.    
    inverse_label_mapping.reserve(max_label + 2);
    inverse_label_mapping.resize(max_label + 1);
    int num_labels = new_label_ids.size();
    vector<int> pruned_labels;
    for (int label = 0; label < num_labels; ++label) {
        if (new_label_ids[label] >= 0) {
            inverse_label_mapping[new_label_ids[label]] = other.inverse_label_mapping[label];
        } else {
            const vector<int> &covered_labels = other.inverse_label_mapping[label];
            pruned_labels.insert(pruned_labels.end(), covered_labels.begin(), covered_labels.end());
        }
    }
    // Map all pruned labels to the same label ID.
    if (!pruned_labels.empty()) {
        inverse_label_mapping.push_back(pruned_labels);
    }
}

int AbstractionFunction::get_abstract_state_id(const State &state) const {
    int index = 0;

    for (size_t i = 0; i < pattern.size(); ++i) {
        index += hash_multipliers[i] * state[pattern[i]].get_value();
    }

    if (state_mapping.empty()) {
        return index;
    }
    assert(utils::in_bounds(index, state_mapping));
    return state_mapping[index];
}

const vector<int> &AbstractionFunction::get_represented_operators(int label) const {
    return inverse_label_mapping[label];
}

void AbstractionFunction::dump() const {
    cout << "Abstraction with pattern: " << pattern << endl;
    cout << "  hash multipliers: " << hash_multipliers << endl;
    cout << "  state mapping: " << state_mapping << endl;
    cout << "  inverse label mapping: " << inverse_label_mapping << endl;
}

double PatternDatabase::get_value(const State &state) const {
    int index = 0;
    for (size_t i = 0; i < pattern.size(); ++i) {
        index += hash_multipliers[i] * state[pattern[i]].get_value();
    }
    return distances[index];
}

//PatternDatabase Abstraction::extract_pdb(const vector<double> &label_weights) const {
//    auto is_negative = [](double w) {return w < 0;};
//    vector<double> distances_alive_states =
//        get_distances(transition_system, label_weights, is_negative);
//    vector<double> distances;
//    distances.reserve(abstraction_function.state_mapping.size());
//    for (int id : abstraction_function.state_mapping) {
//        double dist = abstraction_cut::IS_UNSOLVABLE;
//        if (id != -1) {
//            dist = distances_alive_states[id];
//        }
//        distances.push_back(dist);
//    }
//    return PatternDatabase(abstraction_function.pattern, distances, abstraction_function.hash_multipliers);
//}

/*
  Note that this is different from AbstractionFunction::get_abstract_state
  because here we assume that state is a state in the projection. This
  duplicates some code but we wouldn't want to create abstract states in
  AbstractionFunction::get_abstract_state just to avoid this.
 */
static int rank_state(const vector<int> &hash_multipliers, const vector<int> &state) {
    int index = 0;
    for (size_t i = 0; i < state.size(); ++i) {
        index += hash_multipliers[i] * state[i];
    }
    return index;
}

static void multiply_out_aux(
    const vector<FactPair> &partial_state, const VariablesProxy &variables,
    vector<int> &state, int var, int partial_state_pos,
    function<void(const vector<int> &)> callback) {
    if (var == static_cast<int>(variables.size())) {
        callback(state);
    } else if (partial_state_pos < static_cast<int>(partial_state.size()) &&
               partial_state[partial_state_pos].var == var) {
        state[var] = partial_state[partial_state_pos].value;
        multiply_out_aux(partial_state, variables, state, var + 1, partial_state_pos + 1, callback);
    } else {
        int num_values = variables[var].get_domain_size();
        for (int value = 0; value < num_values; ++value) {
            state[var] = value;
            multiply_out_aux(partial_state, variables, state, var + 1, partial_state_pos, callback);
        }
    }
}

static void multiply_out(const vector<FactPair> &partial_state,
                         const TaskProxy &task_proxy,
                         function<void(const vector<int> &)> callback) {
    // assert(utils::is_sorted_unique(partial_state));
    VariablesProxy variables = task_proxy.get_variables();
    vector<int> state(variables.size());
    multiply_out_aux(partial_state, variables, state, 0, 0, callback);
}

static vector<int> rank_goal_states(const TaskProxy &task_proxy,
                                    const vector<int> &hash_multipliers,
                                    int num_states) {
    GoalsProxy goals = task_proxy.get_goals();
    vector<int> goal_states;
    if (goals.empty()) {
        /*
          In a projection to non-goal variables all states are goal states. We
          treat this as a special case to avoid unnecessary effort multiplying
          out all states.
        */
        goal_states.resize(num_states);
        iota(goal_states.begin(), goal_states.end(), 0);
    } else {
        vector<FactPair> goal_pairs;
        goal_pairs.reserve(goals.size());
        for (FactProxy goal : goals) {
            goal_pairs.push_back(goal.get_pair());
        }
        sort(goal_pairs.begin(), goal_pairs.end());
        multiply_out(goal_pairs, task_proxy, [&](const vector<int> &state) {
                         goal_states.push_back(rank_state(hash_multipliers, state));
                     });
    }
    return goal_states;
}

using OperatorIDsByPreEffCost = utils::HashMap<tuple<vector<FactPair>, vector<FactPair>, bool>, vector<int>>;
static OperatorIDsByPreEffCost group_equivalent_operators(const TaskProxy &task_proxy) {
    OperatorIDsByPreEffCost grouped_operator_ids;
    for (OperatorProxy op : task_proxy.get_operators()) {
        vector<FactPair> preconditions = task_properties::get_fact_pairs(op.get_preconditions());
        sort(preconditions.begin(), preconditions.end());
        vector<FactPair> effects;
        effects.reserve(op.get_effects().size());
        for (EffectProxy eff : op.get_effects()) {
            effects.push_back(eff.get_fact().get_pair());
        }
        sort(effects.begin(), effects.end());
        grouped_operator_ids[make_tuple(preconditions, effects, op.get_cost() == 0)].push_back(op.get_id());
    }
    return grouped_operator_ids;
}

static void rank_transitions(
        const TaskProxy &task_proxy, const vector<int> &hash_multipliers, int num_states,
        vector<Transition> &transitions, vector<int> &offsets, vector<vector<int>> &inverse_label_mapping) {
    OperatorIDsByPreEffCost grouped_operator_ids = group_equivalent_operators(task_proxy);
    inverse_label_mapping.reserve(grouped_operator_ids.size());
    for (const auto &entry : grouped_operator_ids) {
        const auto &pre_eff_cost = entry.first;
        const vector<FactPair> &preconditions = get<0>(pre_eff_cost);
        const vector<FactPair> &effects = get<1>(pre_eff_cost);
        bool is_zero_cost = get<2>(pre_eff_cost);
        const vector<int> &operator_ids = entry.second;

        if (!effects.empty()) {
            // we ignore all self-loops and actions without effect incur self-loops
            vector<int> label_ids;
            label_ids.push_back(static_cast<int>(inverse_label_mapping.size()));
            inverse_label_mapping.push_back(operator_ids);
            multiply_out(preconditions, task_proxy, [&](const vector<int> &state) {
                int state_id = rank_state(hash_multipliers, state);
                vector<int> successor_state(state);
                for (FactPair eff : effects) {
                    successor_state[eff.var] = eff.value;
                }
                int successor_state_id = rank_state(hash_multipliers, successor_state);
                if(successor_state_id != state_id) {
                    // we ignore all self-loops and this effect does nothing
                    for (int label_id: label_ids) {
                        transitions.push_back({-1, state_id, label_id, successor_state_id, is_zero_cost});
                    }
                }
            });
        }
    }

    // TODO: The following code is used almost identically 3 times. Move to a function.
    sort(transitions.begin(), transitions.end(),
         [] (const Transition &lhs, const Transition &rhs) {
             if (lhs.dst == rhs.dst) {
                 if (lhs.is_zero_cost == rhs.is_zero_cost) {
                     return lhs.src < rhs.src;
                 }
                 return lhs.is_zero_cost;
             }
             return lhs.dst < rhs.dst;
         });

    offsets.reserve(num_states + 1);
    offsets.push_back(0);
    int transition_id = 0;
    for (int state_id = 0; state_id < num_states; ++state_id) {
        while ((transition_id < static_cast<int>(transitions.size())) &&
               (transitions[transition_id].dst == state_id)) {
            ++transition_id;
        }
        offsets.push_back(transition_id);
    }
}

Abstraction project_task(
        const shared_ptr<AbstractTask> &task, const vector<int> &pattern) {
    extra_tasks::ProjectedTask projection(task, pattern);
    TaskProxy task_proxy(projection);

    VariablesProxy variables = task_proxy.get_variables();
    int num_states = 1;
    vector<int> hash_multipliers;
    hash_multipliers.reserve(variables.size());
    for (VariableProxy var : variables) {
        hash_multipliers.push_back(num_states);
        if (utils::is_product_within_limit(num_states, var.get_domain_size(),
                                           numeric_limits<int>::max())) {
            num_states *= var.get_domain_size();
        } else {
            cerr << "Given pattern is too large! (Overflow occured): " << endl;
            cerr << pattern << endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }

    vector<int> goal_states = rank_goal_states(task_proxy, hash_multipliers, num_states);

    vector<Transition> transitions;
    vector<int> offsets;
    vector<vector<int>> inverse_label_mapping;
    rank_transitions(task_proxy, hash_multipliers, num_states, transitions, offsets, inverse_label_mapping);
    int num_labels = inverse_label_mapping.size();

    AbstractionFunction alpha {pattern, hash_multipliers, move(inverse_label_mapping)};
    TransitionSystem ts(num_states, num_labels, move(transitions), move(goal_states));
    return { move(alpha), move(ts) };
}

Abstraction prune_dead_parts(Abstraction &&abstraction, const TaskProxy &task_proxy) {
    const TransitionSystem &ts = abstraction.transition_system;
    AbstractionFunction &alpha = abstraction.abstraction_function;
    int initial_state = alpha.get_abstract_state_id(task_proxy.get_initial_state());
    vector<bool> state_alive = compute_alive_states(ts, initial_state);

    if (!state_alive[initial_state]) {
        // Task is unsolvable.
        // TODO: We could return a trivially unsolvable abstraction here.
        ABORT("Support for unsolvable tasks not implemented");
    }

    int num_states = ts.num_states;
    vector<int> new_state_ids(num_states, -1);
    int state_id = 0;
    for (int s = 0; s < num_states; ++s) {
        if (state_alive[s]) {
            new_state_ids[s] = state_id;
            ++state_id;
        }
    }

    vector<bool> label_alive = compute_alive_labels(ts, state_alive);
    int num_labels = ts.num_labels;
    vector<int> new_label_ids(num_labels, -1);
    int label_id = 0;
    for (int label = 0; label < num_labels; ++label) {
        if (label_alive[label]) {
            new_label_ids[label] = label_id;
            ++label_id;
        }
    }

    TransitionSystem new_ts = prune_transition_system(ts, new_state_ids, new_label_ids);
    AbstractionFunction new_alpha(move(alpha), new_state_ids, new_label_ids);
    return Abstraction(move(new_alpha), move(new_ts));
}


Abstraction create_abstraction(const std::shared_ptr<AbstractTask> &task,
                               const pdbs::Pattern &pattern) {
    return prune_dead_parts(project_task(task, pattern), TaskProxy(*task));
}

vector<pdbs::Pattern> create_patterns_from_options(
    const shared_ptr<AbstractTask> &task,
    const plugins::Options &opts, const string &option_name) {
    vector<pdbs::Pattern> patterns;
    vector<shared_ptr<pdbs::PatternCollectionGenerator>> pattern_generators =
        opts.get_list<shared_ptr<pdbs::PatternCollectionGenerator>>(option_name);
    for (const shared_ptr<pdbs::PatternCollectionGenerator> &pattern_generator: pattern_generators) {
        pdbs::PatternCollectionInformation pattern_collection_info =
                pattern_generator->generate(task);
        shared_ptr<vector<pdbs::Pattern>> patterns_of_generator = pattern_collection_info.get_patterns();
        patterns.insert(patterns.end(), patterns_of_generator->begin(), patterns_of_generator->end());
    }
    return patterns;
}

void get_nonzero_cost_predecessors_and_operators(
        const Abstraction &abstraction, int abstract_state_id, const vector<bool> &exclude_state,
        set<int> &predecessors, set<int> &operators) {
    const TransitionSystem &ts = abstraction.transition_system;
    const AbstractionFunction &alpha = abstraction.abstraction_function;

    set<int> transition_ids = get_nonzero_cost_incoming_transitions(
        ts, abstract_state_id, exclude_state);
    for (int transition_id: transition_ids) {
        int label_id = ts.transitions[transition_id].label;
        operators.insert(alpha.get_represented_operators(label_id).begin(),
                         alpha.get_represented_operators(label_id).end());
        predecessors.insert(ts.transitions[transition_id].src);
    }
}

void get_nonzero_cost_successors_and_operators(
    const Abstraction &abstraction, int abstract_state_id, const vector<bool> &exclude_state,
    set<int> &successors, set<int> &operators) {
    const TransitionSystem &ts = abstraction.transition_system;
    const AbstractionFunction &alpha = abstraction.abstraction_function;

    set<int> transition_ids = get_nonzero_cost_outgoing_transitions(
        ts, abstract_state_id, exclude_state);
    for (int transition_id: transition_ids) {
        int label_id = ts.transitions[transition_id].label;
        operators.insert(alpha.get_represented_operators(label_id).begin(),
                         alpha.get_represented_operators(label_id).end());
        successors.insert(ts.transitions[transition_id].dst);
    }
}
}
