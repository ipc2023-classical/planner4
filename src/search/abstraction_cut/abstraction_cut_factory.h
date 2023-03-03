#ifndef ABSTRACTION_CUT_ABSTRACTION_CUT_FACTORY_H
#define ABSTRACTION_CUT_ABSTRACTION_CUT_FACTORY_H

#include "../landmarks/dalm_graph_factory.h"
#include "../plugins/options.h"

#include <set>

namespace abstraction_cut {
struct Abstraction;
using Abstractions = std::vector<Abstraction>;

extern std::vector<std::pair<std::set<int>,std::set<int>>>
    compute_forward_landmarks(const Abstraction &abstraction, State init);
extern std::vector<std::pair<std::set<int>,std::set<int>>>
    compute_backward_landmarks(const Abstraction &abstraction);
extern std::vector<std::pair<std::set<int>,std::set<int>>>
    compute_backward_transition_landmarks(const Abstraction &abstraction);

class AbstractionCutFactory : public landmarks::LandmarkGraphFactory {
    Abstractions abstractions;
    bool backward_lms;
    bool forward_lms;

public:
    AbstractionCutFactory(const plugins::Options &opts);

    virtual void initialize(const std::shared_ptr<AbstractTask> &/*original_task*/) {}
    virtual std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> get_landmark_graph(const State &state);
};
}

#endif
