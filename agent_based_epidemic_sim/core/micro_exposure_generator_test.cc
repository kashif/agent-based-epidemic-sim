#include "agent_based_epidemic_sim/core/micro_exposure_generator.h"

#include "agent_based_epidemic_sim/core/event.h"
#include "agent_based_epidemic_sim/core/exposure_generator.h"
#include "agent_based_epidemic_sim/core/micro_exposure_generator_builder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

const Visit kInfectiousHost = {.infectivity = 1.0, .symptom_factor = 1.0};
const Visit kSusceptibleHost = {.infectivity = 0.0, .symptom_factor = 0.0};

const std::vector<std::vector<float>> kDistribution = {{1.0f}};

TEST(MicroExposureGeneratorTest, CorrectlyDrawsFromDistribution) {
  MicroExposureGeneratorBuilder meg_builder;
  std::unique_ptr<ExposureGenerator> generator =
      meg_builder.Build(kDistribution);

  Exposure exposure_a, exposure_b;
  generator->Generate(1.0, kInfectiousHost, kSusceptibleHost, &exposure_a,
                      &exposure_b);

  ProximityTrace proximity_trace = exposure_a.proximity_trace;
  EXPECT_EQ(proximity_trace.values[0], 1.0f);
  for (int i = 1; i < proximity_trace.values.size(); ++i) {
    EXPECT_EQ(proximity_trace.values[i], std::numeric_limits<float>::max());
  }
}

TEST(MicroExposureGeneratorTest, ProximityTracesEqual) {
  MicroExposureGeneratorBuilder meg_builder;
  std::unique_ptr<ExposureGenerator> generator =
      meg_builder.Build(kDistribution);

  Exposure exposure_a, exposure_b;
  generator->Generate(1.0, kInfectiousHost, kSusceptibleHost, &exposure_a,
                      &exposure_b);

  EXPECT_EQ(exposure_a.proximity_trace, exposure_b.proximity_trace);
}

TEST(MicroExposureGeneratorTest, CorrectOrderingOfExposures) {
  MicroExposureGeneratorBuilder meg_builder;
  std::unique_ptr<ExposureGenerator> generator =
      meg_builder.Build(kDistribution);

  Exposure exposure_a, exposure_b;

  generator->Generate(1.0, kInfectiousHost, kSusceptibleHost, &exposure_a,
                      &exposure_b);

  EXPECT_EQ(exposure_a.infectivity, kSusceptibleHost.infectivity);
  EXPECT_EQ(exposure_b.infectivity, kInfectiousHost.infectivity);

  EXPECT_EQ(exposure_a.symptom_factor, kSusceptibleHost.symptom_factor);
  EXPECT_EQ(exposure_b.symptom_factor, kInfectiousHost.symptom_factor);
}

}  // namespace
}  // namespace abesim
