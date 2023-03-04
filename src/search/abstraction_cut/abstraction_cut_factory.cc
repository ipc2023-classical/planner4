#include "abstraction_cut_factory.h"

#include "projections.h"

#include "../landmarks/dalm_graph.h"
#include "../plugins/plugin.h"

#include <queue>

using namespace std;

namespace abstraction_cut {
namespace {
vector<bool> get_forward_unreachable_states(
    const TransitionSystem &ts, int abstract_state_id) {
    vector<bool> is_goal_state(ts.num_states);
    for (int abstract_goal_id : ts.goal_states) {
        is_goal_state[abstract_goal_id] = true;
    }
    vector<bool> unreachable(ts.num_states, true);
    deque<int> queue;
    unreachable[abstract_state_id] = false;
    if (!is_goal_state[abstract_state_id]) {
        queue.push_back(abstract_state_id);
    }

    while (!queue.empty()) {
        int state_id = queue.front();
        queue.pop_front();
        for (int transition_id = ts.forward_offsets[state_id]; transition_id < ts.forward_offsets[state_id+1]; ++transition_id) {
            int succ_id = ts.forward_transitions[transition_id]->dst;
            if (unreachable[succ_id]) {
                unreachable[succ_id] = false;
                if (!is_goal_state[succ_id]) {
                    queue.push_back(succ_id);
                }
            }
        }
    }
    return unreachable;
}

void process_backward_frontier(
    const TransitionSystem &ts, set<int> &frontier, vector<bool> &goal_zone) {
    for (int frontier_state : frontier) {
        assert(!goal_zone[frontier_state]);
        goal_zone[frontier_state] = true;
    }
    set<int> expanded_frontier;
    for (int frontier_state : frontier) {
        get_zero_cost_predecessors(
            ts, frontier_state, goal_zone, expanded_frontier);
    }
    if (!expanded_frontier.empty()) {
        process_backward_frontier(ts, expanded_frontier, goal_zone);
        frontier.insert(expanded_frontier.begin(), expanded_frontier.end());
    }
}

bool compute_backward_landmarks(const Abstraction &abstraction,
                                const State &state,
                                std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> &result) {
    const TransitionSystem &ts = abstraction.transition_system;
    const AbstractionFunction &alpha = abstraction.abstraction_function;
    int cur_state_id = alpha.get_abstract_state_id(state);
    if (cur_state_id == -1) {
        result->mark_as_dead_end();
        return true;
    }
    vector<bool> goal_zone = get_forward_unreachable_states(ts, cur_state_id);
    set<int> frontier; // TODO: Change to dynamic_bitset?
    for (int goal_state: ts.goal_states) {
        if (!goal_zone[goal_state]) {
            frontier.insert(goal_state);
        }
    }
    process_backward_frontier(ts, frontier, goal_zone);
    int previous_lm_id = -1;
    while (!goal_zone[cur_state_id]) {
        set<int> landmark;
        set<int> next_frontier;
        for (int frontier_state : frontier) {
            get_nonzero_cost_predecessors_and_operators(
                abstraction, frontier_state, goal_zone, next_frontier, landmark);
        }
        process_backward_frontier(ts, next_frontier, goal_zone);

        int current_lm_id = result->add_node(landmark, false);
        if (previous_lm_id != -1) {
            // TODO: some orderings can be shown to be stronger than weak
            result->add_edge(current_lm_id, previous_lm_id, false);
        }
        previous_lm_id = current_lm_id;
        frontier.swap(next_frontier);
    }
    return false;
}

void process_forward_frontier(
    const TransitionSystem &ts, set<int> &frontier, vector<bool> &init_zone) {
    for (int frontier_state : frontier) {
        assert(!init_zone[frontier_state]);
        init_zone[frontier_state] = true;
    }
    set<int> expanded_frontier;
    for (int frontier_state : frontier) {
        get_zero_cost_successors(
            ts, frontier_state, init_zone, expanded_frontier);
    }
    if (!expanded_frontier.empty()) {
        process_forward_frontier(ts, expanded_frontier, init_zone);
        frontier.insert(expanded_frontier.begin(), expanded_frontier.end());
    }
}

bool compute_forward_landmarks(const Abstraction &abstraction,
                               const State &state,
                               std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> &result) {
    const TransitionSystem &ts = abstraction.transition_system;
    const AbstractionFunction &alpha = abstraction.abstraction_function;
    int cur_state_id = alpha.get_abstract_state_id(state);
    if (cur_state_id == -1) {
        result->mark_as_dead_end();
        return true;
    }
    vector<bool> init_zone(ts.num_states, false);
    set<int> frontier; // TODO: Change to dynamic_bitset?
    frontier.insert(cur_state_id);
    process_forward_frontier(ts, frontier, init_zone);
    int previous_lm_id = -1;

    while (all_of(ts.goal_states.begin(), ts.goal_states.end(),
                   [&init_zone](int goal) {return !init_zone[goal];})) {
        set<int> landmark;
        set<int> next_frontier;
        for (int frontier_state : frontier) {
            get_nonzero_cost_successors_and_operators(
                abstraction, frontier_state, init_zone, next_frontier, landmark);
        }
        process_forward_frontier(ts, next_frontier, init_zone);

        int current_lm_id = result->add_node(landmark, false);
        if (previous_lm_id != -1) {
            // TODO: some orderings can be shown to be stronger than weak
            result->add_edge(previous_lm_id, current_lm_id, false);
        }
        previous_lm_id = current_lm_id;
        frontier.swap(next_frontier);
    }
    return false;
}
}

vector<pair<set<int>,set<int>>> compute_backward_landmarks(const Abstraction &abstraction) {
    vector<pair<set<int>,set<int>>> result;
    const TransitionSystem &ts = abstraction.transition_system;

    vector<bool> goal_zone(ts.num_states, false);
    set<int> frontier; // TODO: Change to dynamic_bitset?
    frontier.insert(ts.goal_states.begin(), ts.goal_states.end());
    process_backward_frontier(ts, frontier, goal_zone);

    while (true) {
        set<int> landmark;
        set<int> next_frontier;
        for (int frontier_state : frontier) {
            get_nonzero_cost_predecessors_and_operators(
                    abstraction, frontier_state, goal_zone, next_frontier, landmark);
        }
        process_backward_frontier(ts, next_frontier, goal_zone);

        if (next_frontier.empty()) {
            assert(landmark.empty());
            break;
        }
        assert(!landmark.empty());
        result.push_back(make_pair(next_frontier, landmark));
        frontier.swap(next_frontier);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

vector<pair<set<int>,set<int>>> compute_backward_transition_landmarks(
    const Abstraction &abstraction) {
    vector<pair<set<int>,set<int>>> result;
    const TransitionSystem &ts = abstraction.transition_system;

    vector<bool> goal_zone(ts.num_states, false);
    set<int> frontier; // TODO: Change to dynamic_bitset?
    frontier.insert(ts.goal_states.begin(), ts.goal_states.end());
    process_backward_frontier(ts, frontier, goal_zone);

    while (true) {
        set<int> transition_landmark;
        set<int> next_frontier;
        for (int frontier_state : frontier) {
            set<int> transition_ids =
                get_nonzero_cost_incoming_transitions(ts, frontier_state, goal_zone);
            for (int transition_id : transition_ids) {
                next_frontier.insert(ts.transitions[transition_id].src);
            }
            transition_landmark.insert(transition_ids.begin(), transition_ids.end());
        }
        process_backward_frontier(ts, next_frontier, goal_zone);

        if (next_frontier.empty()) {
            assert(transition_landmark.empty());
            break;
        }
        assert(!transition_landmark.empty());
        result.push_back(make_pair(next_frontier, transition_landmark));
        frontier.swap(next_frontier);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

vector<pair<set<int>,set<int>>> compute_forward_landmarks(const Abstraction &abstraction, State init) {
    vector<pair<set<int>,set<int>>> result;
    const TransitionSystem &ts = abstraction.transition_system;
    const AbstractionFunction &alpha = abstraction.abstraction_function;
    int cur_state_id = alpha.get_abstract_state_id(init);
    if (cur_state_id == -1) {
        return result;
    }
    vector<bool> init_zone(ts.num_states, false);
    set<int> frontier; // TODO: Change to dynamic_bitset?
    frontier.insert(cur_state_id);
    process_forward_frontier(ts, frontier, init_zone);

    while (all_of(ts.goal_states.begin(), ts.goal_states.end(),
                  [&init_zone](int goal) {return !init_zone[goal];})) {
        set<int> landmark;
        set<int> next_frontier;
        for (int frontier_state : frontier) {
            get_nonzero_cost_successors_and_operators(
                    abstraction, frontier_state, init_zone, next_frontier, landmark);
        }
        process_forward_frontier(ts, next_frontier, init_zone);

        result.push_back(make_pair(frontier, landmark));
        frontier.swap(next_frontier);
    }
    return result;
}

AbstractionCutFactory::AbstractionCutFactory(
    const plugins::Options &opts)
    : backward_lms(opts.get<bool>("backward_lms")),
      forward_lms(opts.get<bool>("forward_lms")) {
    std::shared_ptr<AbstractTask> task = opts.get<shared_ptr<AbstractTask>>("transform");
    vector<pdbs::Pattern> patterns = create_patterns_from_options(task, opts, "patterns");

    abstractions.reserve(patterns.size());
    for (const pdbs::Pattern &pattern : patterns) {
        abstractions.push_back(move(create_abstraction(task, pattern)));
    }

    cout << "Number of abstractions: " << abstractions.size() << endl;
}

std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph>
AbstractionCutFactory::compute_landmark_graph(const shared_ptr<AbstractTask> &task) {
    const TaskProxy task_proxy(*task);
    const State &initial_state = task_proxy.get_initial_state();
    return get_landmark_graph(initial_state);
}

shared_ptr<landmarks::DisjunctiveActionLandmarkGraph>
AbstractionCutFactory::get_landmark_graph(const State &state) {
    shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> result =
            utils::make_unique_ptr<landmarks::DisjunctiveActionLandmarkGraph>();
    bool is_dead_end = false;
    for (const Abstraction &abstraction : abstractions) {
        if (backward_lms) {
            is_dead_end = compute_backward_landmarks(abstraction, state, result);
            if (is_dead_end) {
                break;
            }
        }
        if (forward_lms) {
            is_dead_end = compute_forward_landmarks(abstraction, state, result);
            if (is_dead_end) {
                break;
            }
        }
    }
    return result;
}

//static shared_ptr<landmarks::LandmarkGraphFactory> _parse(OptionParser &parser) {
//    parser.document_synopsis(
//        "Disjunctive action landmark graph factory based on abstractions.", "");
//
//    parser.add_list_option<shared_ptr<pdbs::PatternCollectionGenerator>>(
//            "patterns",
//            "pattern generation methods",
//            "[systematic(2)]");
//    parser.add_option<bool>(
//        "backward_lms",
//        "compute backward landmarks",
//        "true");
//    parser.add_option<bool>(
//        "forward_lms",
//        "compute forward landmarks",
//        "false");
//
//    // TODO: Once the heuristic that uses this is implemented, it can pass the task to this
//    parser.add_option<shared_ptr<AbstractTask>>(
//            "transform",
//            "Optional task transformation for the heuristic."
//            " Currently, adapt_costs() and no_transform() are available.",
//            "no_transform()");
//
//    Options opts = parser.parse();
//    if (parser.help_mode() || parser.dry_run()) {
//        return nullptr;
//    }
//
//    return make_shared<AbstractionCutFactory>(opts);
//}

class AbstractionCutFactoryFeature
    : public plugins::TypedFeature<landmarks::LandmarkGraphFactory, AbstractionCutFactory> {
public:
    AbstractionCutFactoryFeature()
        : TypedFeature("abstraction_cut") {
        document_title("TODO");
        document_synopsis(
                "Disjunctive action landmark graph factory based on abstractions.");

        add_list_option<shared_ptr<pdbs::PatternCollectionGenerator>>(
                "patterns",
                "pattern generation methods",
                "[systematic(2)]");
        add_option<bool>(
                "backward_lms",
                "compute backward landmarks",
                "true");
        add_option<bool>(
                "forward_lms",
                "compute forward landmarks",
                "false");

        // TODO: Once the heuristic that uses this is implemented, it can pass the task to this
        add_option<shared_ptr<AbstractTask>>(
                "transform",
                "Optional task transformation for the heuristic."
                " Currently, adapt_costs() and no_transform() are available.",
                "no_transform()");
    }
};

static plugins::FeaturePlugin<AbstractionCutFactoryFeature> _plugin;

}