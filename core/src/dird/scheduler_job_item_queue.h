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

struct JobItem {
  RunResource* run{nullptr};
  JobResource* job{nullptr};
  time_t runtime{0};
  int priority{10};
};

class PrioritiseJobItems {
 public:
  bool operator()(const JobItem& a, const JobItem& b) const;
};

using PrioritisedJobItemsQueue =
    std::priority_queue<JobItem, std::vector<JobItem>, PrioritiseJobItems>;


}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_
