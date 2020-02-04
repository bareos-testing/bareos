#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

# pistache
# add_subdirectory(pistache)

# gtest
set(BUILD_SHARED_LIBS yes)
set(INSTALL_GTEST OFF CACHE BOOL "do not install gtest")

add_subdirectory(googletest)
set (GTEST_MAIN_LIBRARIES gtest_main PARENT_SCOPE)
set (GTEST_LIBRARIES gtest PARENT_SCOPE)
set (GTEST_MAIN_FOUND TRUE PARENT_SCOPE)
set (GTEST_FOUND TRUE PARENT_SCOPE)

# lmdb
if(HAVE_FREEBSD_OS)
  add_definitions(-D_WANT_SEMUN=1)
endif()

add_library(bareoslmdb SHARED
  lmdb/libraries/liblmdb/mdb.c
  lmdb/libraries/liblmdb/midl.c)

# empty the include_directories
set_property(TARGET bareoslmdb PROPERTY INCLUDE_DIRECTORIES "")

set_target_properties(
  bareoslmdb PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}" SOVERSION
                        "${BAREOS_VERSION_MAJOR}"
)
if(HAVE_WIN32)
  set_target_properties(bareoslmdb PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
  target_link_libraries(bareoslmdb -lntdll)
endif()

install(TARGETS bareoslmdb DESTINATION ${libdir})
