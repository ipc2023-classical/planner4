#include "landmark_status_manager.h"

#include "landmark.h"

using namespace std;

namespace landmarks {
/*
  By default we mark all landmarks past, since we do an intersection when
  computing new landmark information.
*/
LandmarkStatusManager::LandmarkStatusManager(
    LandmarkGraph &graph,
    bool progress_goals,
    bool progress_greedy_necessary_orderings,
    bool progress_reasonable_orderings)
    : lm_graph(graph),
      progress_goals(progress_goals),
      progress_greedy_necessary_orderings(progress_greedy_necessary_orderings),
      progress_reasonable_orderings(progress_reasonable_orderings),
      past_lms(vector<bool>(graph.get_num_landmarks(), true)),
      future_lms(vector<bool>(graph.get_num_landmarks(), false)),
      lm_status(graph.get_num_landmarks(), FUTURE) {
}

BitsetView LandmarkStatusManager::get_past_landmarks(const State &state) {
    return past_lms[state];
}

BitsetView LandmarkStatusManager::get_future_landmarks(const State &state) {
    return future_lms[state];
}

void LandmarkStatusManager::process_initial_state(
    const State &initial_state, utils::LogProxy &log) {
    /*
    int num_bits = lm_graph.get_num_landmarks();
    int num_blocks = BitsetMath::compute_num_blocks(num_bits);
    vector<BitsetMath::Block> packed_bits_past(num_blocks, 0);
    BitsetView none_past(ArrayView<BitsetMath::Block>(
        packed_bits_past.data(), num_blocks), num_bits);
    vector<BitsetMath::Block> packed_bits_fut(num_blocks, 0);
    BitsetView all_fut(ArrayView<BitsetMath::Block>(
        packed_bits_fut.data(), num_blocks), num_bits);
    none_past.reset();
    all_fut.set();
    */

    set_landmarks_for_initial_state(initial_state, log);
    update_lm_status(initial_state);
}

void LandmarkStatusManager::set_landmarks_for_initial_state(
    const State &initial_state, utils::LogProxy & /*log*/) {
    BitsetView past = get_past_landmarks(initial_state);
    BitsetView fut = get_future_landmarks(initial_state);
    for (auto &node : lm_graph.get_nodes()) {
        int id = node->get_id();
        if (node->get_landmark().is_true_in_state(initial_state)) {
            assert(past.test(id));
            for (auto &parent : node->parents) {
                utils::unused_variable(parent);
                assert(parent.second <= EdgeType::REASONABLE);
                /* Reasonable orderings for which both landmarks hold should
                   not be generated in the first place. */
                assert(!parent.first->get_landmark().is_true_in_state(
                    initial_state));
                fut.set(id);
            }
        } else {
            past.reset(id);
            fut.set(id);
        }
    }

    /* TODO: Log #goal-lms and #init-lms. (I don't think anybody should care.)

    if (log.is_at_least_normal()) {
        log << inserted << " initial landmarks, "
            << num_goal_lms << " goal landmarks" << endl;
    }
    */
}

void LandmarkStatusManager::process_state_transition(
    const State &parent_ancestor_state, OperatorID,
    const State &ancestor_state) {
    if (ancestor_state == parent_ancestor_state) {
        // This can happen, e.g., in Satellite-01.
        return;
    }

    const BitsetView parent_past = get_past_landmarks(parent_ancestor_state);
    BitsetView past = get_past_landmarks(ancestor_state);

    const BitsetView parent_fut = get_future_landmarks(parent_ancestor_state);
    BitsetView fut = get_future_landmarks(ancestor_state);

    int num_landmarks = lm_graph.get_num_landmarks();
    assert(past.size() == num_landmarks);
    assert(parent_past.size() == num_landmarks);
    assert(fut.size() == num_landmarks);
    assert(parent_fut.size() == num_landmarks);

    progress_basic(parent_past, parent_fut, parent_ancestor_state, past,
                   fut, ancestor_state);

    for (int id = 0; id < num_landmarks; ++id) {
        if (progress_goals) {
            progress_goal(id, ancestor_state, fut);
        }
        if (progress_greedy_necessary_orderings) {
            progress_greedy_necessary(id, ancestor_state, past, fut);
        }
        if (progress_reasonable_orderings) {
            progress_reasonable(id, past, fut);
        }
    }
}

void LandmarkStatusManager::progress_basic(
    const BitsetView &parent_past, const BitsetView &parent_fut,
    const State &parent_ancestor_state, BitsetView &past, BitsetView &fut,
    const State &ancestor_state) {

    utils::unused_variable(parent_fut);
    utils::unused_variable(parent_ancestor_state);
    for (int id = 0; id < lm_graph.get_num_landmarks(); ++id) {
        if (!parent_past.test(id)) {
            assert(parent_fut.test(id));
            assert(!lm_graph.get_node(id)->get_landmark().is_true_in_state(
                parent_ancestor_state));
            /* TODO: Computing whether a landmark is true in a state is
                expensive. It can happen that we compute this multiple times for
                the same state. Maybe we can find a way to circumvent that. */
            if (past.test(id)
                && !lm_graph.get_node(id)->get_landmark().is_true_in_state(
                    ancestor_state)) {
                // Found a path where LM_id does did not yet hold.
                past.reset(id);
                fut.set(id);
            }
        }
    }
}

void LandmarkStatusManager::progress_goal(
    int id, const State &ancestor_state, BitsetView &fut) {
    if (!fut.test(id)) {
        Landmark &lm = lm_graph.get_node(id)->get_landmark();
        if (lm.is_true_in_goal && !lm.is_true_in_state(ancestor_state)) {
            ++goal_progression_counter;
            fut.set(id);
        }
    }
}

void LandmarkStatusManager::progress_greedy_necessary(
    int id, const State &ancestor_state, const BitsetView &past,
    BitsetView &fut) {
    if (past.test(id)) {
        return;
    }

    for (auto &parent : lm_graph.get_node(id)->parents) {
        if (parent.second != EdgeType::GREEDY_NECESSARY
            || fut.test(parent.first->get_id())) {
            continue;
        }
        if (!parent.first->get_landmark().is_true_in_state(ancestor_state)) {
            ++gn_progression_counter;
            fut.set(parent.first->get_id());
        }
    }
}

void LandmarkStatusManager::progress_reasonable(
    int id, const BitsetView &past, BitsetView &fut) {
    if (past.test(id)) {
        return;
    }

    for (auto &child : lm_graph.get_node(id)->children) {
        if (child.second == EdgeType::REASONABLE) {
            ++reasonable_progression_counter;
            fut.set(child.first->get_id());
        }
    }
}

void LandmarkStatusManager::update_lm_status(const State &ancestor_state) {
    // TODO: We don't need the detour over *lm_status* anymore.

    const BitsetView past = get_past_landmarks(ancestor_state);
    const BitsetView fut = get_future_landmarks(ancestor_state);

    const int num_landmarks = lm_graph.get_num_landmarks();
    for (int id = 0; id < num_landmarks; ++id) {
        if (!past.test(id)) {
            assert(fut.test(id));
            lm_status[id] = FUTURE;
        } else if (!fut.test(id)) {
            assert(past.test(id));
            lm_status[id] = PAST;
        } else {
            lm_status[id] = PAST_AND_FUTURE;
        }
    }

    ///* This first loop is necessary as setup for the needed-again
    //   check in the second loop. */
    //for (int id = 0; id < num_landmarks; ++id) {
    //    lm_status[id] = past.test(id) ? PAST : FUTURE;
    //}
    //for (int id = 0; id < num_landmarks; ++id) {
    //    if (lm_status[id] == PAST
    //        && landmark_needed_again(id, ancestor_state)) {
    //        lm_status[id] = PAST_AND_FUTURE;
    //    }
    //}
}
}
