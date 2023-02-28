#ifndef LANDMARKS_LANDMARK_STATUS_MANAGER_H
#define LANDMARKS_LANDMARK_STATUS_MANAGER_H

#include "dalm_graph.h"

#include "../per_state_bitset.h"

namespace landmarks {
class LandmarkGraph;
class LandmarkNode;

enum LandmarkStatus {PAST = 0, FUTURE = 1, PAST_AND_FUTURE = 2};

class DisjunctiveActionLandmarkStatusManager {
    DisjunctiveActionLandmarkGraph &lm_graph;
    const bool progress_goals;
    const bool progress_greedy_necessary_orderings;
    const bool progress_weak_orderings;

    PerStateBitset past_lms;
    PerStateBitset future_lms;

    void progress_basic(
        const BitsetView &parent_past, const BitsetView &parent_fut,
        BitsetView &past, BitsetView &fut, int op_id);
    void progress_goal(const State &ancestor_state, BitsetView &fut);
    void progress_greedy_necessary(const State &ancestor_state,
                                   const BitsetView &past, BitsetView &fut);
    void progress_weak(const BitsetView &past, BitsetView &fut);
public:
    explicit DisjunctiveActionLandmarkStatusManager(
        DisjunctiveActionLandmarkGraph &graph,
        bool progress_goals,
        bool progress_greedy_necessary_orderings,
        bool progress_weak_orderings);

    BitsetView get_past_landmarks(const State &state);
    BitsetView get_future_landmarks(const State &state);

    void process_initial_state(
        const State &initial_state, utils::LogProxy &log);
    void process_state_transition(
        const State &parent_ancestor_state, OperatorID op_id,
        const State &ancestor_state);

    LandmarkStatus get_landmark_status(const State &ancestor_state, size_t id);
};
}

#endif
