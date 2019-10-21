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
#include "dird.h"
#include "dird/run_hour_validator.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/scheduler_system_time_source.h"
#include "dird/scheduler_time_adapter.h"
#include "dird/storage.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"

#include <atomic>
#include <chrono>

namespace directordaemon {

class DefaultSchedulerTimeAdapter : public SchedulerTimeAdapter {
 public:
  DefaultSchedulerTimeAdapter()
      : SchedulerTimeAdapter(std::make_unique<SystemTimeSource>())
  {
  }
};

static std::unique_ptr<SchedulerTimeAdapter> time_adapter{
    std::make_unique<DefaultSchedulerTimeAdapter>()};

static std::function<void(JobControlRecord*)> ExecuteJobCallback{ExecuteJob};

void OverrideSchedulerDefaults(
    std::unique_ptr<SchedulerTimeAdapter> ta,
    std::function<void(JobControlRecord*)> ExecuteJob)
{
  time_adapter = std::forward<std::unique_ptr<SchedulerTimeAdapter>>(ta);
  ExecuteJobCallback = ExecuteJob;
}

const int debuglevel = 200;
static std::atomic<bool> active{true};
static SchedulerJobItemQueue prioritised_job_item_queue;

static void AddJobsForThisAndNextHourToQueue();
static void AddJobToQueue(JobResource* job,
                          RunResource* run,
                          time_t now,
                          time_t runtime);
static void SetJcrFromRunResource(JobControlRecord* jcr, RunResource* run);

static bool JobIsDisabled(JobResource* job)
{
  if (!job->schedule) { return true; }
  if (!job->schedule->enabled) { return true; }
  if (!job->enabled) { return true; }
  if (job->client && !job->client->enabled) { return true; }
  return false;
}

static JobControlRecord* TryCreateJobControlRecord(SchedulerJobItem& next_job)
{
  if (JobIsDisabled(next_job.job_)) {
    return nullptr;
  } else {
    next_job.run_->last_run = time_adapter->time_source_->SystemTime();
    JobControlRecord* jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
    SetJcrDefaults(jcr, next_job.job_);
    SetJcrFromRunResource(jcr, next_job.run_);
    Dmsg0(debuglevel, "Leave SchedulerWaitForNextJob()\n");
    return jcr;
  }
}

static void WaitForJobsToRun()
{
  SchedulerJobItem next_job;

  while (active && !prioritised_job_item_queue.Empty()) {
    next_job = prioritised_job_item_queue.TakeOutTopItem();
    if (!next_job.is_valid_) { break; }
    bool job_started = false;
    while (!job_started) {
      time_t now = time_adapter->time_source_->SystemTime();
      time_t wait = next_job.runtime_ - now;
      if (wait <= 0) {
        JobControlRecord* jcr = TryCreateJobControlRecord(next_job);
        if (jcr) { ExecuteJobCallback(jcr); }
        job_started = true;
      } else {
        time_t wait_interval{time_adapter->default_wait_interval_ < wait
                                 ? time_adapter->default_wait_interval_
                                 : wait};
        time_adapter->time_source_->WaitFor(
            std::chrono::seconds(wait_interval));
      }
    }
  }
}

static void FillSchedulerJobQueue()
{
  while (active && prioritised_job_item_queue.Empty()) {
    AddJobsForThisAndNextHourToQueue();
    if (prioritised_job_item_queue.Empty()) {
      time_adapter->time_source_->WaitFor(
          std::chrono::seconds(time_adapter->default_wait_interval_));
    }
  }
}

void RunScheduler()
{
  active = true;
  while (active) {
    Dmsg0(debuglevel, "Enter SchedulerWaitForNextJob\n");
    FillSchedulerJobQueue();
    WaitForJobsToRun();
  }
  prioritised_job_item_queue.Clear();
}

void TerminateScheduler()
{
  active = false;
  time_adapter->time_source_->Terminate();
}

void ClearSchedulerQueue(void)
{
  // Clear() is thread safe
  prioritised_job_item_queue.Clear();
}


static time_t CalculateRuntime(time_t time, uint32_t minute)
{
  struct tm tm;
  Blocaltime(&time, &tm);
  tm.tm_min = minute;
  tm.tm_sec = 0;
  return mktime(&tm);
}

static void AddJobsForThisAndNextHourToQueue()
{
  Dmsg0(debuglevel, "enter AddJobsForThisAndNextHourToQueue()\n");

  RunHourValidator this_hour(time_adapter->time_source_->SystemTime());
  this_hour.PrintDebugMessage(debuglevel);

  RunHourValidator next_hour(this_hour.Time() + 3600);
  next_hour.PrintDebugMessage(debuglevel);

  JobResource* job = nullptr;

  LockRes(my_config);
  foreach_res (job, R_JOB) {
    if (JobIsDisabled(job)) { continue; }

    Dmsg1(debuglevel, "Got job: %s\n", job->resource_name_);

    for (RunResource* run = job->schedule->run; run; run = run->next) {
      bool run_this_hour = this_hour.TriggersOn(run->date_time_bitfield);
      bool run_next_hour = next_hour.TriggersOn(run->date_time_bitfield);

      Dmsg3(debuglevel, "run@%p: run_now=%d run_next_hour=%d\n", run,
            run_this_hour, run_next_hour);

      if (run_this_hour || run_next_hour) {
        time_t runtime = CalculateRuntime(this_hour.Time(), run->minute);
        if (run_this_hour) {
          AddJobToQueue(job, run, this_hour.Time(), runtime);
        }
        if (run_next_hour) {
          AddJobToQueue(job, run, this_hour.Time(), runtime + 3600);
        }
      }
    }
  }
  UnlockRes(my_config);
  Dmsg0(debuglevel, "Leave AddJobsForThisAndNextHourToQueue()\n");
}

static void AddJobToQueue(JobResource* job,
                          RunResource* run,
                          time_t now,
                          time_t runtime)
{
  if (((runtime - run->last_run) < 61) || ((runtime + 59) < now)) { return; }

  try {
    prioritised_job_item_queue.EmplaceItem(job, run, runtime);
  } catch (const std::invalid_argument& e) {
    Dmsg1(debuglevel, "Could not emplace job: %s\n", e.what());
  }
}

static void SetJcrFromRunResource(JobControlRecord* jcr, RunResource* run)
{
  if (run->level) { jcr->setJobLevel(run->level); /* override run level */ }

  if (run->pool) {
    jcr->res.pool = run->pool; /* override pool */
    jcr->res.run_pool_override = true;
  }

  if (run->full_pool) {
    jcr->res.full_pool = run->full_pool; /* override full pool */
    jcr->res.run_full_pool_override = true;
  }

  if (run->vfull_pool) {
    jcr->res.vfull_pool = run->vfull_pool; /* override virtual full pool */
    jcr->res.run_vfull_pool_override = true;
  }

  if (run->inc_pool) {
    jcr->res.inc_pool = run->inc_pool; /* override inc pool */
    jcr->res.run_inc_pool_override = true;
  }

  if (run->diff_pool) {
    jcr->res.diff_pool = run->diff_pool; /* override diff pool */
    jcr->res.run_diff_pool_override = true;
  }

  if (run->next_pool) {
    jcr->res.next_pool = run->next_pool; /* override next pool */
    jcr->res.run_next_pool_override = true;
  }

  if (run->storage) {
    UnifiedStorageResource store;
    store.store = run->storage;
    PmStrcpy(store.store_source, _("run override"));
    SetRwstorage(jcr, &store); /* override storage */
  }

  if (run->msgs) { jcr->res.messages = run->msgs; /* override messages */ }

  if (run->Priority) { jcr->JobPriority = run->Priority; }

  if (run->spool_data_set) { jcr->spool_data = run->spool_data; }

  if (run->accurate_set) {
    jcr->accurate = run->accurate; /* overwrite accurate mode */
  }

  if (run->MaxRunSchedTime_set) { jcr->MaxRunSchedTime = run->MaxRunSchedTime; }
}

} /* namespace directordaemon */
