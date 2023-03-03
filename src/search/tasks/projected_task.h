#ifndef TASKS_PROJECTED_TASK_H
#define TASKS_PROJECTED_TASK_H

#include "delegating_task.h"

#include "../pdbs/types.h"

#include <vector>

namespace extra_tasks {
class ProjectedTask : public tasks::DelegatingTask {
    pdbs::Pattern pattern;
    std::vector<FactPair> goals;
    std::vector<std::vector<FactPair>> operator_preconditions;
    std::vector<std::vector<FactPair>> operator_effects;
public:
    ProjectedTask(
        const std::shared_ptr<AbstractTask> &parent,
        const pdbs::Pattern &pattern);

    virtual int get_num_variables() const override;
    virtual std::string get_variable_name(int var) const override;
    virtual int get_variable_domain_size(int var) const override;
    virtual std::string get_fact_name(const FactPair &fact) const override;
    virtual bool are_facts_mutex(
        const FactPair &fact1, const FactPair &fact2) const override;

    virtual int get_num_operator_preconditions(int index, bool is_axiom) const override;
    virtual FactPair get_operator_precondition(
        int op_index, int fact_index, bool is_axiom) const override;
    virtual int get_num_operator_effects(int op_index, bool is_axiom) const override;
    virtual FactPair get_operator_effect(
        int op_index, int eff_index, bool is_axiom) const override;

    virtual int get_num_goals() const override;
    virtual FactPair get_goal_fact(int index) const override;

    virtual std::vector<int> get_initial_state_values() const override;

    virtual void convert_state_values_from_parent(std::vector<int> &) const override;
};
}

#endif
