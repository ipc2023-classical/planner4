#ifndef ABSTRACTION_CUT_PROJECTIONS
#define ABSTRACTION_CUT_PROJECTIONS

#include "transition_system.h"

#include "../pdbs/pattern_generator.h"

namespace abstraction_cut {
/*
  We consider abstractions that correspond to projections with dead parts removed.
  The abstraction function maps states from the original task to abstract states.
 */
struct Abstraction;

class AbstractionFunction {
    friend struct Abstraction;
    // List of variable IDs from the original task
    std::vector<int> pattern;

    /*
      Multipliers are used for perfect hashing: states of the original task are
      mapped to IDs that are unique within the projection. The resulting IDs
      range from 0 to the product of all domain sizes for variables in the
      pattern.
     */
    std::vector<int> hash_multipliers;


    /*
      The actual abstract states are numbered 0 to n, so when dead states are
      removed, the abstraction has fewer states than perfect hashing assumes.
      The following function maps IDs from perfect hashing to abstract state
      IDs. If state_mapping is empty, we assume the identity mapping.
     */
    std::vector<int> state_mapping;
    /*
      Each label can represent multiple operators. We map the ID of the label
      to the set of operator IDs of represented operators. An operator that is
      not represented by any label is irrelevant for the abstraction, i.e.,
      induces self-loops on all states. We intentionally do *not* add labels for
      such operators.
    */
    std::vector<std::vector<int>> inverse_label_mapping;

public:
    AbstractionFunction(const pdbs::Pattern &pattern,
                        const std::vector<int> &hash_multipliers,
                        std::vector<std::vector<int>> &&inverse_label_mapping);

    // Coarsening of the given abstraction function
    AbstractionFunction(AbstractionFunction &&other,
                        const std::vector<int> &new_state_ids,
                        const std::vector<int> &new_label_ids);

    int get_abstract_state_id(const State &state) const;
    const std::vector<int> &get_represented_operators(int label) const;
    const std::vector<int> &get_pattern() const {
        return pattern;
    }

    size_t get_num_labels() const {
        return inverse_label_mapping.size();
    }
    void dump() const;
};

class PatternDatabase {
    // List of variable IDs from the original task
    std::vector<int> pattern;

    /*
      final h-values for abstract-states.
    */
    std::vector<double> distances;

    // multipliers for each variable for perfect hash function
    std::vector<int> hash_multipliers;
public:
    PatternDatabase(std::vector<int> _pattern, std::vector<double> _distances,
                    std::vector<int> _hash_multipliers)
        : pattern(_pattern),
          distances(_distances),
          hash_multipliers(_hash_multipliers) {}

    double get_value(const State &state) const;
};

struct Abstraction {
    AbstractionFunction abstraction_function;
    TransitionSystem transition_system;
    Abstraction(AbstractionFunction &&abstraction_function, TransitionSystem &&transition_system)
        : abstraction_function(std::move(abstraction_function)),
          transition_system(std::move(transition_system)) {
    }

    void dump() const {
        abstraction_function.dump();
    }

    // PatternDatabase extract_pdb(const std::vector<double> &label_weights) const;
};

/*
  Creates a projection to the given pattern with dead states removed.
  If combine_labels is true, the abstract transition system will not us the IDs
  of the original operators but instead use new labels where each label
  represents a set of operators with parallel transitions.
 */
Abstraction create_abstraction(
    const std::shared_ptr<AbstractTask> &task, const pdbs::Pattern &pattern);

extern Abstraction prune_dead_parts(
    Abstraction &&abstraction, const TaskProxy &task_proxy);

std::vector<pdbs::Pattern> create_patterns_from_options(
    const std::shared_ptr<AbstractTask> &task,
    const plugins::Options &opts, const std::string &option_name);

void get_nonzero_cost_predecessors_and_operators(
    const Abstraction &abstraction, int abstract_state_id,
    const std::vector<bool> &exclude_state,
    std::set<int> &predecessors, std::set<int> &operators);
void get_nonzero_cost_successors_and_operators(
    const Abstraction &abstraction, int abstract_state_id,
    const std::vector<bool> &exclude_state,
    std::set<int> &successors, std::set<int> &operators);
}

#endif
