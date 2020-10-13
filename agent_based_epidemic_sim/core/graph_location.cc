// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "agent_based_epidemic_sim/core/graph_location.h"

#include <algorithm>
#include <memory>
#include <random>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"
#include "agent_based_epidemic_sim/core/random.h"

namespace abesim {

namespace {

class GraphLocation : public Location {
 public:
  GraphLocation(int64 uuid, std::function<float()> location_transmissibility,
                float drop_probability,
                std::vector<std::pair<int64, int64>> graph,
                const ExposureGenerator& exposure_generator)
      : graph_(std::move(graph)),
        uuid_(uuid),
        location_transmissibility_(std::move(location_transmissibility)),
        drop_probability_(drop_probability),
        exposure_generator_(exposure_generator) {}

  int64 uuid() const override { return uuid_; }

  void ProcessVisits(absl::Span<const Visit> visits,
                     Broker<InfectionOutcome>* infection_broker) override {
    thread_local absl::flat_hash_map<int64, Visit> visit_map;
    visit_map.clear();
    for (const Visit& v : visits) {
      visit_map[v.agent_uuid] = v;
    }

    MaybeUpdateGraph(visits);

    absl::BitGenRef gen = GetBitGen();

    for (const std::pair<int64, int64>& edge : graph_) {
      // Randomly drop some potential contacts.
      if (absl::Bernoulli(gen, drop_probability_)) continue;

      // If either of the participants are not present, no contact is generated.
      auto visit_a = visit_map.find(edge.first);
      if (visit_a == visit_map.end()) continue;
      auto visit_b = visit_map.find(edge.second);
      if (visit_b == visit_map.end()) continue;

      InfectionOutcome outcome_a = {
          .agent_uuid = edge.first,
          .exposure_type = InfectionOutcomeProto::CONTACT,
          .source_uuid = edge.second,
      };
      InfectionOutcome outcome_b = {
          .agent_uuid = edge.second,
          .exposure_type = InfectionOutcomeProto::CONTACT,
          .source_uuid = edge.first,
      };
      exposure_generator_.Generate(location_transmissibility_(),
                                   visit_a->second, visit_b->second,
                                   &outcome_a.exposure, &outcome_b.exposure);
      infection_broker->Send({outcome_a, outcome_b});
    }
  }

 protected:
  std::vector<std::pair<int64, int64>> graph_;

 private:
  virtual void MaybeUpdateGraph(absl::Span<const Visit> visits) {}

  const int64 uuid_;
  const std::function<float()> location_transmissibility_;
  const float drop_probability_;
  const ExposureGenerator& exposure_generator_;
};

class RandomGraphLocation : public GraphLocation {
 public:
  RandomGraphLocation(int64 uuid,
                      std::function<float()> location_transmissibility,
                      const ExposureGenerator& exposure_generator)
      : GraphLocation(uuid, std::move(location_transmissibility),
                      /* drop_probability = */ 0.0, /* graph = */ {},
                      exposure_generator) {}

 private:
  void MaybeUpdateGraph(absl::Span<const Visit> visits) override {
    // Construct list of agent UUIDs as potential endpoints for edges. An agent
    // is repeated once for each edge needed by it.
    thread_local std::vector<int64> agent_uuids;
    internal::AgentUuidsFromRandomLocationVisits(visits, agent_uuids);
    // Connect random pairs till none remain.
    std::shuffle(agent_uuids.begin(), agent_uuids.end(), GetBitGen());
    internal::ConnectAdjacentNodes(agent_uuids, graph_);
  }
};

}  // namespace

namespace internal {

void AgentUuidsFromRandomLocationVisits(absl::Span<const Visit> visits,
                                        std::vector<int64>& agent_uuids) {
  agent_uuids.clear();
  for (const Visit& visit : visits) {
    agent_uuids.resize(
        agent_uuids.size() + visit.location_dynamics.random_location_edges,
        visit.agent_uuid);
  }
}

void ConnectAdjacentNodes(absl::Span<const int64> agent_uuids,
                          std::vector<std::pair<int64, int64>>& graph) {
  graph.clear();
  while (agent_uuids.size() >= 2) {
    int64 a = agent_uuids[0];
    int64 b = agent_uuids[1];
    if (a == b) {
      // Self-edges are not allowed. Try next pair.
      agent_uuids.remove_prefix(1);
      continue;
    }
    if (a > b) std::swap(a, b);
    graph.emplace_back(a, b);
    agent_uuids.remove_prefix(2);
  }
  // Remove duplicate edges.
  std::sort(graph.begin(), graph.end());
  auto dups = std::unique(graph.begin(), graph.end());
  graph.erase(dups, graph.end());
}

}  // namespace internal

std::unique_ptr<Location> NewGraphLocation(
    int64 uuid, std::function<float()> location_transmissibility,
    float drop_probability, std::vector<std::pair<int64, int64>> graph,
    const ExposureGenerator& exposure_generator) {
  return absl::make_unique<GraphLocation>(
      uuid, std::move(location_transmissibility), drop_probability,
      std::move(graph), exposure_generator);
}

std::unique_ptr<Location> NewRandomGraphLocation(
    int64 uuid, std::function<float()> location_transmissibility,
    const ExposureGenerator& exposure_generator) {
  return absl::make_unique<RandomGraphLocation>(
      uuid, std::move(location_transmissibility), exposure_generator);
}

}  // namespace abesim
