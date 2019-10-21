/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, May MM, major revision December MMIII
 */
/**
 * @file
 * BAREOS scheduler
 *
 * It looks at what jobs are to be run and when
 * and waits around until it is time to
 * fire them up.
 */

#include "include/bareos.h"
#include "scheduler.h"
#include "dird/scheduler_private.h"
#include "dird/scheduler_time_adapter.h"
#include "include/make_unique.h"

class JobControlRecord;

namespace directordaemon {

class JobResource;
class SchedulerTimeAdapter;

static Scheduler scheduler;
static const int debuglevel = 200;

Scheduler& GetMainScheduler() { return scheduler; }

Scheduler::Scheduler() : impl_(std::make_unique<SchedulerPrivate>()){};

Scheduler::Scheduler(std::unique_ptr<SchedulerTimeAdapter> time_adapter,
                     std::function<void(JobControlRecord*)> ExecuteJob)
    : impl_(std::make_unique<SchedulerPrivate>(
          std::forward<std::unique_ptr<SchedulerTimeAdapter>>(time_adapter),
          std::forward<std::function<void(JobControlRecord*)>>(ExecuteJob)))
{
}

Scheduler::~Scheduler() = default;

void Scheduler::AddJobWithNoRunResourceToQueue(JobResource* job)
{
  impl_->AddJobToQueue(job);
}

void Scheduler::Run()
{
  while (impl_->active_) {
    Dmsg0(debuglevel, "Scheduler Cycle\n");
    impl_->FillSchedulerJobQueue();
    impl_->WaitForJobsToRun();
  }
}

void Scheduler::Terminate()
{
  impl_->active_ = false;
  impl_->time_adapter_->time_source_->Terminate();
}

void Scheduler::ClearQueue(void) { impl_->prioritised_job_item_queue_.Clear(); }

} /* namespace directordaemon */
