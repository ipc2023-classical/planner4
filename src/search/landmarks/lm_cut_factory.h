#ifndef LANDMARKS_LM_CUT_FACTORY_H
#define LANDMARKS_LM_CUT_FACTORY_H

#include "dalm_graph_factory.h"

#include "../plugins/options.h"

namespace landmarks {
//class LandmarkCutLandmarks;

class LMCutFactory : public LandmarkGraphFactory {
    std::map<std::set<int>, size_t> ids; // ??
    //std::unique_ptr<LandmarkCutLandmarks> landmark_generator;

public:
    explicit LMCutFactory(const plugins::Options &opts);

    virtual std::shared_ptr<DisjunctiveActionLandmarkGraph> compute_landmark_graph(
        const std::shared_ptr<AbstractTask> &task) override;
};
}

#endif
