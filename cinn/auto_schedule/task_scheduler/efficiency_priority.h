// Copyright (c) 2022 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <string>

#include "cinn/auto_schedule/task_scheduler/task_scheduler.h"

namespace cinn {
namespace auto_schedule {

// Schedule tasks with efficiency_priority strategy, that
// is picking a task with the maximum earnings ratio.
class EfficiencyPriority : public TaskScheduler {
 public:
  EfficiencyPriority(const std::vector<TuneTask>& tasks, const Config& config) : TaskScheduler(tasks, config) {}

  const char* Name() const override { return "efficiency_priority"; };

 protected:
  int NextTaskId() override;

 private:
  bool IsTaskToTune(const TuneTask* task);
};

}  // namespace auto_schedule
}  // namespace cinn
