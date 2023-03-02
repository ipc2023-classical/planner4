#include "lm_cut_factory.h"

//#include "landmark_factory.h"

#include <algorithm>

#include "../heuristics/lm_cut_landmarks.h"
#include "../plugins/plugin.h"

using namespace std;
using LCL = lm_cut_heuristic::LandmarkCutLandmarks;

namespace landmarks {

LMCutFactory::LMCutFactory(const plugins::Options & /*opts*/) {}

shared_ptr<DisjunctiveActionLandmarkGraph> LMCutFactory::compute_landmark_graph(
    const shared_ptr<AbstractTask> &task) {
    const TaskProxy task_proxy(*task);
    const State &initial_state = task_proxy.get_initial_state();
    LCL lmc = LCL(task_proxy);
    vector<set<int>> landmarks;
    lmc.compute_landmarks(initial_state, nullptr,
                          [&landmarks](const vector<int> &lm, int /*cost*/) {
                              landmarks.emplace_back(lm.begin(), lm.end());
                          });

    dalm_graph graph = make_shared<DisjunctiveActionLandmarkGraph>();
    // Add nodes.
    for (const set<int> &landmark : landmarks) {
        graph->add_node(landmark);
    }
    // graph->dump();
    //  TODO: Add edges/orderings.

    if (graph->get_number_of_landmarks() == 0) {
        size_t id = graph->add_node({});
        graph->mark_lm_initially_past(id);
    }

    utils::g_log << "Landmark graph of initial state contains "
                 << graph->get_number_of_landmarks() << endl;
    utils::g_log << "Landmark graph of initial state contains "
                 << graph->get_number_of_orderings() << " orderings of which "
                 << graph->get_number_of_strong_orderings()
                 << " are strong and " << graph->get_number_of_weak_orderings()
                 << " are weak." << endl;
    // graph->dump_dot();
    return graph;
}

class LMCutFactoryFeature
    : public plugins::TypedFeature<LandmarkGraphFactory, LMCutFactory> {
public:
    LMCutFactoryFeature() : TypedFeature("lm_cut_landmarks") {
        document_title("TODO");
        document_synopsis("Generate LM-cut DALMs");
    }
};

static plugins::FeaturePlugin<LMCutFactoryFeature> _plugin;
}
