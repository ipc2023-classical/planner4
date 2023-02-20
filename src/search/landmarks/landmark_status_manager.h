#ifndef LANDMARKS_LANDMARK_STATUS_MANAGER_H
#define LANDMARKS_LANDMARK_STATUS_MANAGER_H

#include "landmark_graph.h"

#include "../per_state_bitset.h"

namespace landmarks {
class LandmarkGraph;
class LandmarkNode;

enum LandmarkStatus {PAST = 0, FUTURE = 1, PAST_AND_FUTURE = 2};

class LandmarkStatusManager {
    LandmarkGraph &lm_graph;
    const bool progress_goals;
    const bool progress_greedy_necessary_orderings;
    const bool progress_reasonable_orderings;

private:
    PerStateBitset past_lms;
    PerStateBitset future_lms;
    // TODO: We don't need the detour over *lm_statys* anymore.
    std::vector<LandmarkStatus> lm_status;

    void set_landmarks_for_initial_state(
        const State &initial_state, utils::LogProxy &log);

    void progress_basic(
        const BitsetView &parent_past, const BitsetView &parent_fut,
        const State &parent_ancestor_state, BitsetView &past, BitsetView &fut,
        const State &ancestor_state);
    void progress_goal(int id, const State &ancestor_state, BitsetView &fut);
    void progress_greedy_necessary(int id, const State &ancestor_state,
                                   const BitsetView &past, BitsetView &fut);
    void progress_reasonable(int id, const BitsetView &past, BitsetView &fut);
public:
    int goal_progression_counter = 0;
    int gn_progression_counter = 0;
    int reasonable_progression_counter = 0;

    explicit LandmarkStatusManager(
        LandmarkGraph &graph,
        bool progress_goals,
        bool progress_greedy_necessary_orderings,
        bool progress_reasonable_orderings);

    BitsetView get_past_landmarks(const State &state);
    BitsetView get_future_landmarks(const State &state);

    void update_lm_status(const State &ancestor_state);

    void process_initial_state(
        const State &initial_state, utils::LogProxy &log);
    void process_state_transition(
        const State &parent_ancestor_state, OperatorID op_id,
        const State &ancestor_state);

    /*
      TODO:
      The status of a landmark is actually dependent on the state. This
      is not represented in the function below. Furthermore, the status
      manager only stores the status for one particular state at a time.

      At the day of writing this comment, this works as
      *update_past_lms()* is always called before the status
      information is used (by calling *get_landmark_status()*).

      It would be a good idea to ensure that the status for the
      desired state is returned at all times, or an error is thrown
      if the desired information does not exist.
     */
    LandmarkStatus get_landmark_status(size_t id) const {
        assert(static_cast<int>(id) < lm_graph.get_num_landmarks());
        return lm_status[id];
    }
};
}

#endif
