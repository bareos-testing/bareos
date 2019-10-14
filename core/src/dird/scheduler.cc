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
#include "dird/broken_down_time.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/storage.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"

#include <atomic>
#include <chrono>
#include <condition_variable>

namespace directordaemon {

class SystemTimeSource : public TimeSource {
 public:
  time_t SystemTime() const override { return time(nullptr); }
};

static SystemTimeSource time_source;
static SchedulerSettings default_scheduler_settings(time_source, 60);
static const SchedulerSettings* scheduler_settings{&default_scheduler_settings};

void SetSchedulerDefaults(const SchedulerSettings* settings)
{
  scheduler_settings = settings;
}

const int debuglevel = 200;

static std::atomic<bool> active{true};
static std::condition_variable wait_condition;
static std::mutex wait_mutex;

static SchedulerJobItemQueue prioritised_job_item_queue;

static bool AddJobsForThisAndNextHourToQueue();
static void AddJobToQueue(JobResource* job,
                          RunResource* run,
                          time_t now,
                          time_t runtime);

void ClearSchedulerQueue(void) { prioritised_job_item_queue.Clear(); }

static bool JobIsDisabled(JobResource* job)
{
  if (!job->schedule) { return true; }
  if (!job->schedule->enabled) { return true; }
  if (!job->enabled) { return true; }
  if (job->client && !job->client->enabled) { return true; }
  return false;
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

static void WaitFor(std::chrono::seconds wait_interval)
{
  std::unique_lock<std::mutex> ul(wait_mutex);
  wait_condition.wait_for(ul, wait_interval);
}

static JobControlRecord* TryCreateJobControlRecord(SchedulerJobItem& next_job)
{
  if (JobIsDisabled(next_job.job_)) {
    return nullptr;
  } else {
    next_job.run_->last_run = scheduler_settings->time_source_.SystemTime();
    JobControlRecord* jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
    SetJcrDefaults(jcr, next_job.job_);
    SetJcrFromRunResource(jcr, next_job.run_);
    Dmsg0(debuglevel, "Leave SchedulerWaitForNextJob()\n");
    return jcr;
  }
}

static void FillJobQueue()
{
  while (active && prioritised_job_item_queue.Empty()) {
    if (!AddJobsForThisAndNextHourToQueue()) {
      WaitFor(std::chrono::seconds(scheduler_settings->default_wait_interval_));
    }
  }
}

static void AwaitAndRunJobs()
{
  while (active && !prioritised_job_item_queue.Empty()) {
    SchedulerJobItem next_job = prioritised_job_item_queue.TakeOutTopItem();
    if (!next_job.is_valid_) { break; }
    time_t now = time_t(time(NULL));
    time_t wait = next_job.runtime_ - now;
    if (wait <= 0) {
      JobControlRecord* jcr = TryCreateJobControlRecord(next_job);
      if (jcr) { ExecuteJob(jcr); }
    } else {
      time_t wait_interval{scheduler_settings->default_wait_interval_ < wait
                               ? scheduler_settings->default_wait_interval_
                               : wait};
      WaitFor(std::chrono::seconds(wait_interval));
    }
  }  // while (active && next_job.is_valid_)
}

void RunScheduler()
{
  active = true;
  while (active) {
    Dmsg0(debuglevel, "Enter SchedulerWaitForNextJob\n");
    FillJobQueue();
    AwaitAndRunJobs();
  }
  prioritised_job_item_queue.Clear();
}

void TerminateScheduler()
{
  active = false;
  wait_condition.notify_one();
}

bool CalculateRun(const BrokenDownTime& b, const RunResource* run)
{
  return BitIsSet(b.hour, run->hour) && BitIsSet(b.mday, run->mday) &&
         BitIsSet(b.wday, run->wday) && BitIsSet(b.month, run->month) &&
         (BitIsSet(b.wom, run->wom) || (b.is_last_week && run->last_set)) &&
         BitIsSet(b.woy, run->woy);
}

static time_t CalculateRuntime(time_t time, uint32_t minute)
{
  struct tm tm;
  Blocaltime(&time, &tm);
  tm.tm_min = minute;
  tm.tm_sec = 0;
  return mktime(&tm);
}

static bool AddJobsForThisAndNextHourToQueue()
{
  Dmsg0(debuglevel, "enter AddJobsForThisAndNextHourToQueue()\n");

  BrokenDownTime now(time(nullptr));

  Dmsg8(debuglevel, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        now.time, now.hour, now.month, now.mday, now.wday, now.wom, now.woy,
        now.yday);

  BrokenDownTime next_hour(now.time + 3600);

  Dmsg8(debuglevel, "nh = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        next_hour.time, next_hour.hour, next_hour.month, next_hour.mday,
        next_hour.wday, next_hour.wom, next_hour.woy, next_hour.yday);

  JobResource* job = nullptr;
  bool job_added = false;

  LockRes(my_config);
  foreach_res (job, R_JOB) {
    if (JobIsDisabled(job)) { continue; }

    Dmsg1(debuglevel, "Got job: %s\n", job->resource_name_);

    for (RunResource* run = job->schedule->run; run; run = run->next) {
      bool run_now = CalculateRun(now, run);
      bool run_nh = CalculateRun(next_hour, run);

      Dmsg3(debuglevel, "run@%p: run_now=%d run_nh=%d\n", run, run_now, run_nh);

      if (run_now || run_nh) {
        time_t runtime = CalculateRuntime(now.time, run->minute);
        if (run_now) { AddJobToQueue(job, run, now.time, runtime); }
        if (run_nh) { AddJobToQueue(job, run, now.time, runtime + 3600); }
        job_added = true;
      }
    }
  }
  UnlockRes(my_config);
  Dmsg0(debuglevel, "Leave AddJobsForThisAndNextHourToQueue()\n");
  return job_added;
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

} /* namespace directordaemon */
