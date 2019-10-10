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

#include "include/bareos.h"
#include "include/make_unique.h"
#include "scheduler_job_item_queue.h"
#include "dird/dird_conf.h"

namespace directordaemon {

struct PrioritiseJobItems {
  bool operator()(const SchedulerJobItem& a, const SchedulerJobItem& b) const
  {
    bool a_runs_before_b = a.runtime_ < b.runtime_;
    bool a_has_higher_priority_than_b =
        a.runtime_ == b.runtime_ && a.priority_ < b.priority_;
    // invert for std::priority_queue sort algorithm
    return !(a_runs_before_b || a_has_higher_priority_than_b);
  }
};

struct SchedulerJobItemQueuePrivate {
  std::mutex mutex;
  std::priority_queue<SchedulerJobItem,
                      std::vector<SchedulerJobItem>,
                      PrioritiseJobItems>
      priority_queue;
};

SchedulerJobItemQueue::SchedulerJobItemQueue()
    : impl_(std::make_unique<SchedulerJobItemQueuePrivate>())
{
}

SchedulerJobItemQueue::~SchedulerJobItemQueue() = default;

SchedulerJobItem SchedulerJobItemQueue::TakeOutTopItem()
{
  SchedulerJobItem job_item;
  std::lock_guard<std::mutex> lg(impl_->mutex);
  if (!impl_->priority_queue.empty()) {
    job_item = impl_->priority_queue.top();
  }
  impl_->priority_queue.pop();
  return job_item;
}

void SchedulerJobItemQueue::EmplaceItem(JobResource* job,
                                        RunResource* run,
                                        time_t runtime)
{
  std::lock_guard<std::mutex> lg(impl_->mutex);
  impl_->priority_queue.emplace(job, run, runtime,
                                run->Priority ? run->Priority : job->Priority);
}

bool SchedulerJobItemQueue::Empty() const
{
  std::lock_guard<std::mutex> lg(impl_->mutex);
  return impl_->priority_queue.empty();
}

void SchedulerJobItemQueue::Clear()
{
  std::lock_guard<std::mutex> lg(impl_->mutex);
  while (!impl_->priority_queue.empty()) { impl_->priority_queue.pop(); }
}

}  // namespace directordaemon
