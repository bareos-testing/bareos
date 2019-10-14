/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/broken_down_time.h"
#include "dird/dird_conf.h"

using namespace directordaemon;

static SchedulerJobItemQueue scheduler_job_item_queue;

TEST(scheduler_job_item_queue, job_item)
{
  SchedulerJobItem item;
  EXPECT_FALSE(item.is_valid_);

  JobResource job;
  RunResource run;

  SchedulerJobItem item_unitialised(&job, &run, time(nullptr), 0);
  EXPECT_TRUE(item_unitialised.is_valid_);
}

TEST(scheduler_job_item_queue, priority_and_time)
{
  time_t now = time(nullptr);

  std::vector<JobResource> job_resources(4);
  std::vector<RunResource> run_resources(job_resources.size());

  for (std::size_t i = 0; i < job_resources.size(); i++) {
    time_t runtime{0};
    switch (i) {
      case 0:
        runtime = now;
        run_resources[i].Priority = 10;
        job_resources[i].selection_type = 1;  // runs first
        break;
      case 1:
        runtime = now + 1;
        run_resources[i].Priority = 10;
        job_resources[i].selection_type = 3;
        break;
      case 2:
        runtime = now + 1;
        run_resources[i].Priority = 11;
        job_resources[i].selection_type = 4;  // runs last
        break;
      case 3:
        runtime = now + 1;
        run_resources[i].Priority = 9;
        job_resources[i].selection_type = 2;
        break;
      default:
        assert(false);
    }
    scheduler_job_item_queue.EmplaceItem(&job_resources[i], &run_resources[i],
                                         runtime);
  }

  int item_position = 1;
  while (!scheduler_job_item_queue.Empty()) {
    SchedulerJobItem job_item = scheduler_job_item_queue.TakeOutTopItem();
    ASSERT_TRUE(job_item.is_valid_);
    ASSERT_EQ(job_item.job_->selection_type, item_position)
        << "selection_type is used as position parameter in this test";
    item_position++;
  }
}

TEST(scheduler_job_item_queue, job_resource_undefined)
{
  bool failed{false};
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(nullptr, &run, 123);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: JobResource is undefined");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(scheduler_job_item_queue, run_resource_undefined)
{
  bool failed{false};
  JobResource job;
  try {
    scheduler_job_item_queue.EmplaceItem(&job, nullptr, 123);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: RunResource is undefined");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(scheduler_job_item_queue, runtime_undefined)
{
  bool failed{false};
  JobResource job;
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(&job, &run, 0);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: runtime is invalid");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(scheduler_job_item_queue, brokendown_time)
{
  BrokenDownTime bt(time(nullptr));
}
