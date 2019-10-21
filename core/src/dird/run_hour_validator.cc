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

#include "include/bareos.h"
#include "dird/run_hour_validator.h"
#include "dird/date_time_bitfield.h"

#include <iostream>
#include <ios>

namespace directordaemon {

static bool IsDayOfYearInLastWeek(int year, int doy)
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

// calculate the current hour of the year
RunHourValidator::RunHourValidator(time_t time) : time_(time)
{
  struct tm tm;
  localtime_r(&time_, &tm);
  hour_ = tm.tm_hour;
  mday_ = tm.tm_mday - 1;
  wday_ = tm.tm_wday;
  month_ = tm.tm_mon;
  wom_ = mday_ / 7;
  woy_ = TmWoy(time_); /* get week of year */
  yday_ = tm.tm_yday;  /* get day of year */
  is_last_week_ = IsDayOfYearInLastWeek(tm.tm_year + 1900, yday_);
}

// check if the calculated hour matches the runtime bitfiled
bool RunHourValidator::TriggersOn(const DateTimeBitfield& bits)
{
  return BitIsSet(hour_, bits.hour) && BitIsSet(mday_, bits.mday) &&
         BitIsSet(wday_, bits.wday) && BitIsSet(month_, bits.month) &&
         (BitIsSet(wom_, bits.wom) ||
          (is_last_week_ && bits.last_week_of_month)) &&
         BitIsSet(woy_, bits.woy);
}

void RunHourValidator::PrintDebugMessage(int debuglevel) const
{
  Dmsg8(debuglevel, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        time_, hour_, month_, mday_, wday_, wom_, woy_, yday_);
}

}  // namespace directordaemon
