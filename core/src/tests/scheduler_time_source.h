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

#ifndef BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_
#define BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_

#include "dird/scheduler.h"
#include "dird/scheduler_time_adapter.h"

#include <atomic>
#include <condition_variable>
#include <thread>
#include <iostream>

static bool debug{false};

class SimulatedTimeSource : public directordaemon::TimeSource {
 public:
  enum class SleepBetweenTicksMode
  {
    kSleep,
    kNoSleep
  };
  SimulatedTimeSource(SleepBetweenTicksMode sleep)
  {
    sleep_mode = sleep;
    wait_until_ = 0;
    wait_until_elapsed_ = false;
    running_ = true;

    clock_value_ = 959817600;  // UTC: 01-06-2000 00:00:00

    time_t t{clock_value_};

    if (debug) {
      std::cout << std::put_time(
                       gmtime(&t),
                       "Start simulated Clock at time: %d-%m-%Y %H:%M:%S")
                << std::endl;
    }
    clock_thread_ = std::thread(ClockRunner);
  }

  ~SimulatedTimeSource()
  {
    //
    clock_thread_.join();
  }

  void WaitFor(std::chrono::seconds wait_interval_pseudo_seconds) override
  {
    wait_until_ = clock_value_ +
                  static_cast<time_t>(wait_interval_pseudo_seconds.count());

    auto timeout_on_error = std::chrono::seconds(
        sleep_mode == SleepBetweenTicksMode::kNoSleep ? 1 : 10);

    std::unique_lock<std::mutex> ul(mutex_);
    wait_condition_.wait_for(ul, timeout_on_error);
  }

  void Terminate() override
  {
    //
    running_ = false;
  }
  time_t SystemTime() override { return clock_value_; }
  static void ExecuteJob(JobControlRecord* jcr);

 private:
  static void ClockRunner()
  {
    // sleep to give the scheduler time to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    while (running_) {
      clock_value_ += 1;
      if (clock_value_ > wait_until_) {
        std::lock_guard<std::mutex> ul(mutex_);
        wait_until_elapsed_ = true;
        wait_condition_.notify_one();
      }

      if (sleep_mode == SleepBetweenTicksMode::kSleep) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } else {
        std::this_thread::yield();
      }
    }
  }
  static SleepBetweenTicksMode sleep_mode;
  std::thread clock_thread_;
  static std::atomic<bool> running_;
  static std::atomic<time_t> wait_until_;
  static bool wait_until_elapsed_;
  static std::atomic<time_t> clock_value_;
  static std::mutex mutex_;
  static std::condition_variable wait_condition_;
};

SimulatedTimeSource::SleepBetweenTicksMode SimulatedTimeSource::sleep_mode;
std::atomic<bool> SimulatedTimeSource::running_{false};
std::atomic<time_t> SimulatedTimeSource::clock_value_{0};
std::atomic<time_t> SimulatedTimeSource::wait_until_{0};
bool SimulatedTimeSource::wait_until_elapsed_{false};
std::mutex SimulatedTimeSource::mutex_;
std::condition_variable SimulatedTimeSource::wait_condition_;

#endif  // BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_
