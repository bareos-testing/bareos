/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_
#define BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_

#include <queue>
#include <vector>

namespace directordaemon {

class RunResource;
class JobResource;
class SchedulerJobItemQueuePrivate;

struct SchedulerJobItem {
  SchedulerJobItem() = default;
  SchedulerJobItem(JobResource* job,
                   RunResource* run,
                   time_t runtime,
                   int priority)
      : job_(job), run_(run), runtime_(runtime), priority_(priority)
  {
    is_valid_ = job && run && runtime;
  };
  JobResource* job_{nullptr};
  RunResource* run_{nullptr};
  time_t runtime_{0};
  int priority_{10};
  bool is_valid_{false};
};

class SchedulerJobItemQueue {
 public:
  SchedulerJobItemQueue();
  ~SchedulerJobItemQueue();

  SchedulerJobItem TakeOutTopItem();
  void EmplaceItem(JobResource* job, RunResource* run, time_t runtime);
  bool Empty() const;
  void Clear();

 private:
  std::unique_ptr<SchedulerJobItemQueuePrivate> impl_;
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_
