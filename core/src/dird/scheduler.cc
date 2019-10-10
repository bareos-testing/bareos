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
#include "dird/scheduler.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/storage.h"
#include "lib/parse_conf.h"

#include <atomic>

namespace directordaemon {

const int debuglevel = 200;
static constexpr int default_wait_interval{60};

static SchedulerJobItemQueue prioritised_job_item_queue;

static bool AddJobsForThisAndNextHourToQueue();
static void AddJobToQueue(JobResource* job,
                          RunResource* run,
                          time_t now,
                          time_t runtime);

void ClearSchedulerQueue(void)
{
  LockJobs();
  prioritised_job_item_queue.Clear();
  UnlockJobs();
}

static bool JobIsDisabled(JobResource* job)
{
  return (!job->enabled || (job->schedule && !job->schedule->enabled) ||
          (job->client && !job->client->enabled));
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

JobControlRecord* SchedulerWaitForNextJob()
{
  Dmsg0(debuglevel, "Enter SchedulerWaitForNextJob\n");

again:
  while (prioritised_job_item_queue.Empty()) {
    if (!AddJobsForThisAndNextHourToQueue()) {
      Bmicrosleep(default_wait_interval, 0);
    }
  }

  SchedulerJobItem next_job = prioritised_job_item_queue.TakeOutTopItem();
  if (!next_job.is_valid_) { goto again; }

  time_t now;

  for (;;) {
    now = time(NULL);
    time_t wait = next_job.runtime_ - now;
    if (wait <= 0) { break; }

    wait = default_wait_interval < wait ? default_wait_interval : wait;
    Bmicrosleep(wait, 0);
  }

  if (JobIsDisabled(next_job.job_)) { goto again; }

  next_job.run_->last_run = now;

  JobControlRecord* jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
  SetJcrDefaults(jcr, next_job.job_);

  SetJcrFromRunResource(jcr, next_job.run_);

  Dmsg0(debuglevel, "Leave SchedulerWaitForNextJob()\n");
  return jcr;
}

void TermScheduler()
{
  // Ueb
}

/**
 * check if given day of year is in last week of the month in the current year
 * depending if the year is leap year or not, the doy of the last day of the
 * month is varying one day.
 */
bool IsDoyInLastWeek(int year, int doy)
{
  int i;
  int* last_dom;
  int last_day_of_month[] = {31,  59,  90,  120, 151, 181,
                             212, 243, 273, 304, 334, 365};
  int last_day_of_month_leap[] = {31,  60,  91,  121, 152, 182,
                                  213, 244, 274, 305, 335, 366};

  /*
   * Determine if this is a leap year.
   */
  if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0)) {
    last_dom = last_day_of_month_leap;
  } else {
    last_dom = last_day_of_month;
  }

  for (i = 0; i < 12; i++) {
    /* doy is zero-based */
    if (doy > ((last_dom[i] - 1) - 7) && doy <= (last_dom[i] - 1)) {
      return true;
    }
  }

  return false;
}

class BreakdownTime {
 public:
  int hour{0};
  int mday{0};
  int wday{0};
  int month{0};
  int wom{0};
  int woy{0};
  int yday{0};
  time_t time{0};
  bool is_last_week{false};

  BreakdownTime(time_t time)
  {
    struct tm tm;
    Blocaltime(&time, &tm);
    hour = tm.tm_hour;
    mday = tm.tm_mday - 1;
    wday = tm.tm_wday;
    month = tm.tm_mon;
    wom = mday / 7;
    woy = TmWoy(time); /* get week of year */
    yday = tm.tm_yday; /* get day of year */
    is_last_week = IsDoyInLastWeek(tm.tm_year + 1900, yday);
  }
};

bool CalculateRun(const BreakdownTime& b, const RunResource* run)
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

  BreakdownTime now(time(nullptr));

  Dmsg8(debuglevel, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        now.time, now.hour, now.month, now.mday, now.wday, now.wom, now.woy,
        now.yday);

  BreakdownTime next_hour(now.time + 3600);

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

  prioritised_job_item_queue.EmplaceItem(job, run, runtime);
}
} /* namespace directordaemon */
