#include "fact_landmark_graph_translator_factory.h"

#include "landmark_factory.h"

#include "../plugins/plugin.h"

using namespace std;

namespace landmarks {
FactLandmarkGraphTranslatorFactory::FactLandmarkGraphTranslatorFactory(
    const plugins::Options &opts)
    : lm(opts.get<std::shared_ptr<LandmarkFactory>>("lm")) {
}

vector<size_t> FactLandmarkGraphTranslatorFactory::add_nodes(
    dalm_graph &graph, const LandmarkGraph &lm_graph, const State &init_state) {
    vector<size_t> fact_to_action_lm_map(lm_graph.get_num_landmarks(), -1);
    for (const unique_ptr<LandmarkNode> &node: lm_graph.get_nodes()) {
        if (!node->get_landmark().is_true_in_state(init_state)
            || !node->parents.empty()) {
            fact_to_action_lm_map[node->get_id()] =
                graph->add_node(node->get_landmark().possible_achievers);
        }
    }
    return fact_to_action_lm_map;
}

void FactLandmarkGraphTranslatorFactory::add_edges(
    dalm_graph &graph, const LandmarkGraph &lm_graph, const State &init_state,
    const vector<size_t> &fact_to_action_lm_map) {

    for (auto &node: lm_graph.get_nodes()) {
        if (node->get_landmark().is_true_in_state(init_state)) {
            /* All edges starting in initially true facts are not
               interesting for us since the cycles they possibly induce
               are already resolved initially. */
            continue;
        }
        size_t from_id = fact_to_action_lm_map[node->get_id()];
        for (std::pair<LandmarkNode *const, EdgeType> &child: node->children) {
            size_t to_id = fact_to_action_lm_map[child.first->get_id()];
            /*
              If there is an action which occurs in both landmarks, applying it
              resolves both landmarks as well as the ordering in one step. This
              special case (which is a consequence of the definition of
              reasonable orderings) makes a lot of things very complicated.
              Ignoring these cases may be desired sometimes which is why we do
              not take them over into our DALM-graph here if the
              *keep_intersecting_orderings* flag is set to false (default).
            */
            if (!graph->landmarks_overlap(from_id, to_id)) {
                graph->add_edge(
                    from_id, to_id, child.second >= EdgeType::NATURAL);
            }
        }
    }
}

shared_ptr<DisjunctiveActionLandmarkGraph> FactLandmarkGraphTranslatorFactory::compute_landmark_graph(
    const shared_ptr<AbstractTask> &task) {
    const TaskProxy task_proxy(*task);
    const State &initial_state = task_proxy.get_initial_state();
    const LandmarkGraph &fact_graph = *lm->compute_lm_graph(task);
    dalm_graph graph = make_shared<DisjunctiveActionLandmarkGraph>();
    const vector<size_t> fact_to_action_lm_map =
        add_nodes(graph, fact_graph, initial_state);
    add_edges(graph, fact_graph, initial_state, fact_to_action_lm_map);

    utils::g_log << "Landmark graph of initial state contains "
                 << graph->get_number_of_landmarks() << endl;
    utils::g_log << "Landmark graph of initial state contains "
                 << graph->get_number_of_orderings()
                 << " orderings of which "
                 << graph->get_number_of_strong_orderings()
                 << " are strong and "
                 << graph->get_number_of_weak_orderings()
                 << " are weak." << endl;
    graph->dump_dot();
    return graph;
}

class FactLandmarkGraphTranslatorFactoryFeature
    : public plugins::TypedFeature<LandmarkGraphFactory, FactLandmarkGraphTranslatorFactory> {
public:
    FactLandmarkGraphTranslatorFactoryFeature()
        : TypedFeature("fact_translator") {
        document_title("TODO");
        document_synopsis(
            "Fact to Disjunctive Action Landmark Graph Translator");
        add_option<shared_ptr<LandmarkFactory>>(
            "lm", "Method to produce landmarks");
    }
};

static plugins::FeaturePlugin<FactLandmarkGraphTranslatorFactoryFeature> _plugin;
}
