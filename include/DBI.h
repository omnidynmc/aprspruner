/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
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
 **************************************************************************/

#ifndef APRSPRUNER_DBI_H
#define APRSPRUNER_DBI_H

#include <openframe/DBI.h>
#include <aprs/APRS.h>

namespace aprspruner {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

#define NULL_OPTIONPP(x, y) ( x->getString(y).length() > 0 ? mysqlpp::SQLTypeAdapter(x->getString(y)) : mysqlpp::SQLTypeAdapter(mysqlpp::null) )
#define NULL_VALID_OPTIONPP(v, s, x, y) (( x->getString(y).length() > 0 && v.is_valid(s, x->getString(y)) )  ? mysqlpp::SQLTypeAdapter(x->getString(y)) : mysqlpp::SQLTypeAdapter(mysqlpp::null) )


/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class DBI : public openframe::DBI {
    public:
      DBI(const openframe::LogObject::thread_id_t thread_id,
          const std::string &db,
          const std::string &host,
          const std::string &user,
          const std::string &pass);
      virtual ~DBI();

      void prepare_queries();

      const size_t deletePacketsByAge(const time_t age, const size_t limit);
      const size_t deleteRawByAge(const time_t age, const size_t limit);

    protected:
    private:
  }; // class DBI

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/

} // namespace aprspruner
#endif
