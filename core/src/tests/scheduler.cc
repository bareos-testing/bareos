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
#include "dird/dird_globals.h"
#include "dird/scheduler.h"
#include "dird/dird_conf.h"
#include "lib/parse_conf.h"

#include <chrono>
#include <thread>

using namespace directordaemon;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

static void StopScheduler(std::chrono::milliseconds timeout)
{
  std::this_thread::sleep_for(timeout);
  TerminateScheduler();
}

TEST(scheduler, cancel_scheduler)
{
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file =
      std::string(PROJECT_SOURCE_DIR "/src/tests/configs/scheduler");

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  my_config->ParseConfig();

  std::thread scheduler_canceler(StopScheduler, std::chrono::milliseconds(200));

  RunScheduler();

  scheduler_canceler.join();
  delete my_config;
}

using namespace std::chrono;

TEST(scheduler, time_compared_with_chrono_system_clock)
{
  time_t c_time_now = time(nullptr);
  const auto& chrono_now = system_clock::now();

  uint32_t t1 = c_time_now;
  uint32_t t2 = duration_cast<seconds>(chrono_now.time_since_epoch()).count();

  EXPECT_LT(t1, t2 + 1);
  EXPECT_GT(t1, t2 - 1);
}

static int counter{0};
class TestTimeSource : public TimeSource {
 public:
  time_t SystemTime() const override
  {
    std::cout << ++counter << std::endl;
    return time(nullptr);
  }
};

static TestTimeSource test_time_source;
static SchedulerSettings scheduler_settings(test_time_source, 1);

TEST(scheduler, schedule_jobs)
{
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file =
      std::string(PROJECT_SOURCE_DIR "/src/tests/configs/scheduler");

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  my_config->ParseConfig();

  SetSchedulerDefaults(&scheduler_settings);

  std::thread scheduler_canceler(StopScheduler,
                                 std::chrono::milliseconds(2000));

  RunScheduler();

  scheduler_canceler.join();
  delete my_config;
}
