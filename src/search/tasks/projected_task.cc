#include "projected_task.h"

#include "../utils/collections.h"
#include "../utils/math.h"
#include "../utils/logging.h"
#include "../utils/system.h"

using namespace std;

namespace extra_tasks {
static bool has_conditional_effects(const AbstractTask &task) {
    int num_ops = task.get_num_operators();
    for (int op_index = 0; op_index < num_ops; ++op_index) {
        int num_effs = task.get_num_operator_effects(op_index, false);
        for (int eff_index = 0; eff_index < num_effs; ++eff_index) {
            int num_conditions = task.get_num_operator_effect_conditions(
                op_index, eff_index, false);
            if (num_conditions > 0) {
                return true;
            }
        }
    }
    return false;
}


ProjectedTask::ProjectedTask(
    const shared_ptr<AbstractTask> &parent, const pdbs::Pattern &pattern)
    : DelegatingTask(parent), pattern(pattern) {
    if (parent->get_num_axioms() > 0) {
        ABORT("ProjectedTask doesn't support axioms.");
    }
    if (has_conditional_effects(*parent)) {
        ABORT("ProjectedTask doesn't support conditional effects.");
    }

    int num_variables = parent->get_num_variables();
    std::vector<int> parent_variable_to_local_variable(num_variables, -1);
    for (int i = 0; i < static_cast<int>(pattern.size()); ++i) {
        parent_variable_to_local_variable[pattern[i]] = i;
    }

    int num_goals = parent->get_num_goals();
    goals.reserve(num_goals);
    for (int i = 0; i < num_goals; ++i) {
        FactPair goal = parent->get_goal_fact(i);
        int var = parent_variable_to_local_variable[goal.var];
        if (var != -1) {
            goals.emplace_back(var, goal.value);
        }
    }
    goals.shrink_to_fit();

    int num_operators = parent->get_num_operators();
    operator_preconditions.resize(num_operators);
    operator_effects.resize(num_operators);
    for (int op_id = 0; op_id < num_operators; ++op_id) {
        vector<FactPair> &preconditions = operator_preconditions[op_id];
        int num_preconditions = parent->get_num_operator_preconditions(op_id, false);
        preconditions.reserve(num_preconditions);
        for (int pre_id = 0; pre_id < num_preconditions; ++pre_id) {
            FactPair pre = parent->get_operator_precondition(op_id, pre_id, false);
            int var = parent_variable_to_local_variable[pre.var];
            if (var != -1) {
                preconditions.emplace_back(var, pre.value);
            }
        }
        preconditions.shrink_to_fit();

        vector<FactPair> &effects = operator_effects[op_id];
        int num_effects = parent->get_num_operator_effects(op_id, false);
        effects.reserve(num_effects);
        for (int eff_id = 0; eff_id < num_effects; ++eff_id) {
            FactPair eff = parent->get_operator_effect(op_id, eff_id, false);
            int var = parent_variable_to_local_variable[eff.var];
            if (var != -1) {
                effects.emplace_back(var, eff.value);
            }
        }
        effects.shrink_to_fit();
    }
}


int ProjectedTask::get_num_variables() const {
    return static_cast<int>(pattern.size());
}

string ProjectedTask::get_variable_name(int var) const {
    assert(utils::in_bounds(var, pattern));
    return parent->get_variable_name(pattern[var]);
}

int ProjectedTask::get_variable_domain_size(int var) const {
    assert(utils::in_bounds(var, pattern));
    return parent->get_variable_domain_size(pattern[var]);
}

string ProjectedTask::get_fact_name(const FactPair &fact) const {
    assert(utils::in_bounds(fact.var, pattern));
    return parent->get_fact_name(FactPair(pattern[fact.var], fact.value));
}

bool ProjectedTask::are_facts_mutex(const FactPair &fact1, const FactPair &fact2) const {
    assert(utils::in_bounds(fact1.var, pattern));
    FactPair parent_fact1(pattern[fact1.var], fact1.value);
    assert(utils::in_bounds(fact2.var, pattern));
    FactPair parent_fact2(pattern[fact2.var], fact2.value);
    return parent->are_facts_mutex(parent_fact1, parent_fact2);
}

int ProjectedTask::get_num_operator_preconditions(int index, bool) const {
    assert(utils::in_bounds(index, operator_preconditions));
    return static_cast<int>(operator_preconditions[index].size());
}

FactPair ProjectedTask::get_operator_precondition(
    int op_index, int fact_index, bool) const {
    assert(utils::in_bounds(op_index, operator_preconditions));
    assert(utils::in_bounds(fact_index, operator_preconditions[op_index]));
    return operator_preconditions[op_index][fact_index];
}

int ProjectedTask::get_num_operator_effects(int op_index, bool) const {
    assert(utils::in_bounds(op_index, operator_effects));
    return static_cast<int>(operator_effects[op_index].size());
}

FactPair ProjectedTask::get_operator_effect(int op_index, int eff_index, bool) const {
    assert(utils::in_bounds(op_index, operator_effects));
    assert(utils::in_bounds(eff_index, operator_effects[op_index]));
    return operator_effects[op_index][eff_index];
}

int ProjectedTask::get_num_goals() const {
    return static_cast<int>(goals.size());
}

FactPair ProjectedTask::get_goal_fact(int index) const {
    assert(utils::in_bounds(index, goals));
    return goals[index];
}

vector<int> ProjectedTask::get_initial_state_values() const {
    vector<int> initial_state_values = parent->get_initial_state_values();
    convert_state_values_from_parent(initial_state_values);
    return initial_state_values;
}

void ProjectedTask::convert_state_values_from_parent(vector<int> &values) const {
    vector<int> abstract_values(pattern.size());
    for (int i = 0; i < static_cast<int>(pattern.size()); ++i) {
        abstract_values[i] = values[pattern[i]];
    }
    values.swap(abstract_values);
}
}
