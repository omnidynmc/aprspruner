/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************
 $Id: Vars.cpp,v 1.1 2005/11/21 18:16:04 omni Exp $
 **************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>
#include <iostream>
#include <string>
#include <exception>
#include <sstream>

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>

#include <openframe/openframe.h>
#include <aprs/APRS.h>

#include "DBI.h"
#include "Validator.h"

namespace aprspruner {
  using namespace openframe::loglevel;


  /**************************************************************************
   ** DBI Class                                                     **
   **************************************************************************/

  /******************************
   ** Constructor / Destructor **
   ******************************/

  DBI::DBI(const openframe::LogObject::thread_id_t thread_id,
                         const std::string &db,
                         const std::string &host,
                         const std::string &user,
                         const std::string &pass)
             : openframe::LogObject(thread_id),
               openframe::DBI(db, host, user, pass) {
  } // DBI::DBI

  DBI::~DBI() {
  } // DBI::~DBI

  void DBI::prepare_queries() {
  } // DBI::prepare_queries

  const size_t DBI::deletePacketsByAge(const time_t age, const size_t limit) {
    size_t num_affected_rows = 0;

    try {
      mysqlpp::Transaction trans(*_sqlpp);
      mysqlpp::Query query = _sqlpp->query();

      query << "DELETE FROM packet WHERE create_ts < "
            << age
            << " LIMIT " << limit;

      query.execute();
      query = _sqlpp->query();

      trans.commit();
      num_affected_rows = query.affected_rows();
    } // try (transaction)
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{Delete::packet}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{Delete::packet}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return num_affected_rows;
  } // DBI::deletePacketsByAge

  const size_t DBI::deleteRawByAge(const time_t age, const size_t limit) {
    size_t num_affected_rows = 0;

    try {
      mysqlpp::Transaction trans(*_sqlpp);
      mysqlpp::Query query = _sqlpp->query();

      query << "DELETE FROM raw WHERE create_ts < "
            << age
            << " LIMIT " << limit;

      query.execute();
      query = _sqlpp->query();

      trans.commit();
      num_affected_rows = query.affected_rows();
    } // try (transaction)
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{Delete::raw}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{Delete::raw}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return num_affected_rows;
  } // DBI::deleteRawByAge


} // namespace aprspruner
