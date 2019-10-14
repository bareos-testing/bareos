/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_SCHEDULER_H_
#define BAREOS_DIRD_SCHEDULER_H_

#include <chrono>

class JobControlRecord;

namespace directordaemon {

class RunResource;
class BrokenDownTime;

class TimeSource {
 public:
  virtual time_t SystemTime() const = 0;
};

struct SchedulerSettings {
  SchedulerSettings(TimeSource& time_source, time_t default_wait_interval)
      : time_source_(time_source), default_wait_interval_(default_wait_interval)
  {
  }

  TimeSource& time_source_;
  time_t default_wait_interval_{0};
};

void RunScheduler();
bool IsDoyInLastWeek(int year, int doy);
void TerminateScheduler();
void ClearSchedulerQueue();
bool CalculateRun(const BrokenDownTime& b, const RunResource* run);
void SetSchedulerDefaults(const SchedulerSettings* settings);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_SCHEDULER_H_
