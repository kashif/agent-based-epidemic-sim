/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_
#define AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "agent_based_epidemic_sim/applications/risk_learning/config.pb.h"
#include "agent_based_epidemic_sim/core/location_type.h"
#include "agent_based_epidemic_sim/core/risk_score.h"

namespace abesim {

// A static representation of the risk score model loaded it at simulation
// start. All usages of this class should be const& to prevent us creating a new
// copy for each agent.
class LearningRiskScoreModel {
 public:
  LearningRiskScoreModel() {}
  LearningRiskScoreModel(
      float overall_real, const std::vector<BLEBucket>& ble_buckets,
      const std::vector<InfectiousnessBucket>& infectiousness_buckets,
      const int exposure_notification_window_days)
      : overall_real_(overall_real),
        ble_buckets_(ble_buckets),
        infectiousness_buckets_(infectiousness_buckets),
        exposure_notification_window_days_(exposure_notification_window_days) {}

  float ComputeRiskScore(
      const Exposure& exposure,
      absl::optional<absl::Time> initial_symptom_onset_time) const;

 private:
  // Inverse form of the RSSI to distance function found in the Cencetti paper:
  // https://www.medrxiv.org/content/10.1101/2020.05.29.20115915v2.
  int DistanceToRSSI(const float distance) const {
    const float alpha = 8.851;
    const float beta = 113.4;
    const float gamma = 3.715;
    return pow(alpha / distance, 1 / gamma) - beta;
  }

  absl::StatusOr<int> RSSIToBinIndex(const int rssi) const;

  float ComputeDurationRiskScore(const Exposure& exposure) const;
  // Note: This method assumes infectiousness_buckets_ has a particular
  // ordering. Specifically the ordering is asc on days_since_symptom_onset_max.
  float ComputeInfectionRiskScore(
      absl::optional<int64> days_since_symptom_onset) const;

  // Overall scaling factor for risk score. This scales the product of duration
  // and infection scores.
  float overall_real_;
  // Buckets representing threshold and corresponding weight of ble attenuation
  // signals.
  std::vector<BLEBucket> ble_buckets_;
  // Buckets representing days_since_symptom onset and a mapping to a
  // corresponding infectiousness level and model weight.
  std::vector<InfectiousnessBucket> infectiousness_buckets_;
  // The number of days of exosure history to use when determining whether to
  // quarantine, test and other policy actions.
  int exposure_notification_window_days_;
};

absl::StatusOr<const LearningRiskScoreModel> CreateLearningRiskScoreModel(
    const LearningRiskScoreModelProto& proto);

absl::StatusOr<std::unique_ptr<RiskScore>> CreateLearningRiskScore(
    const TracingPolicyProto& proto,
    const LearningRiskScoreModel& risk_score_model,
    LocationTypeFn location_type);

}  // namespace abesim

#endif  // AGENT_BASED_EPIDEMIC_SIM_APPLICATIONS_RISK_LEARNING_RISK_SCORE_H_
