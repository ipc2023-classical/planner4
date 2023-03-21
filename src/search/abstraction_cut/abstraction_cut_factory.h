#ifndef ABSTRACTION_CUT_ABSTRACTION_CUT_FACTORY_H
#define ABSTRACTION_CUT_ABSTRACTION_CUT_FACTORY_H
// taoheu

#include "../landmarks/dalm_graph_factory.h"
#include "../plugins/options.h"

#include <set>

namespace abstraction_cut {
struct Abstraction;
using Abstractions = std::vector<Abstraction>;

class AbstractionCutFactory : public landmarks::LandmarkGraphFactory {
    Abstractions abstractions;
    bool backward_lms;
    bool forward_lms;
    bool justification_graph;

public:
    AbstractionCutFactory(const plugins::Options &opts);

    virtual void initialize(const std::shared_ptr<AbstractTask> &/*original_task*/) {}
    std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> get_landmark_graph(const State &state);
    std::shared_ptr<landmarks::DisjunctiveActionLandmarkGraph> compute_landmark_graph(const std::shared_ptr<AbstractTask> &task) override;
};
}

#endif
