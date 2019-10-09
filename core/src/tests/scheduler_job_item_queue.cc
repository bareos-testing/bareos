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
#include "dird/dird_conf.h"

using namespace directordaemon;

static PrioritisedJobItemsQueue prioritised_job_item_queue;

TEST(scheduler_job_item_queue, asd)
{
  time_t now = time(nullptr);

  std::vector<JobResource> job_resources(4);

  for (std::size_t i = 0; i < job_resources.size(); i++) {
    JobItem job_item;
    switch (i) {
      case 0:
        job_item.runtime = now;
        job_item.priority = 10;
        job_resources[i].selection_type = 1;  // runs first
        break;
      case 1:
        job_item.runtime = now + 1;
        job_item.priority = 10;
        job_resources[i].selection_type = 3;
        break;
      case 2:
        job_item.runtime = now + 1;
        job_item.priority = 11;
        job_resources[i].selection_type = 4;  // runs last
        break;
      case 3:
        job_item.runtime = now + 1;
        job_item.priority = 9;
        job_resources[i].selection_type = 2;
        break;
      default:
        assert(false);
    }
    job_item.job = &job_resources[i];
    prioritised_job_item_queue.push(job_item);
  }

  int item_position = 1;
  while (!prioritised_job_item_queue.empty()) {
    const JobItem& job_item = prioritised_job_item_queue.top();
    ASSERT_EQ(job_item.job->selection_type, item_position)
        << "selection_type is used as position parameter in this test";
    prioritised_job_item_queue.pop();
    item_position++;
  }
}
