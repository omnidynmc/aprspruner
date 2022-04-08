#ifndef APRSPRUNER_WORKER_H
#define APRSPRUNER_WORKER_H

#include <string>
#include <vector>
#include <list>

#include <openframe/openframe.h>
#include <openstats/openstats.h>
#include <stomp/Stomp.h>
#include <aprs/APRS.h>

namespace aprspruner {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class MemcachedController;
  class DBI_Inject;
  class Store;


  class Worker_Exception : public openframe::OpenFrame_Exception {
    public:
      Worker_Exception(const std::string message) throw() : openframe::OpenFrame_Exception(message) { };
  }; // class Worker_Exception

  class Worker : public virtual openframe::LogObject,
                 public openstats::StatsClient_Interface {
    public:
      // ### Constants ### //
      static const int kDefaultStompPrefetch;
      static const time_t kDefaultStatsInterval;
      static const time_t kDefaultDeleteInterval;
      static const time_t kDefaultMemcachedExpire;
      static const char *kStompDestErrors;
      static const char *kStompDestRejects;
      static const char *kStompDestDuplicates;
      static const char *kStompDestNotifyMessages;

      // ### Init ### //
      Worker(const openframe::LogObject::thread_id_t thread_id,
             const std::string &stomp_hosts,
             const std::string &stomp_dest,
             const std::string &stomp_login,
             const std::string &stomp_passcode,
             const std::string &db_host,
             const std::string &db_user,
             const std::string &db_pass,
             const std::string &db_database);
      virtual ~Worker();
      void init();
      bool run();
      void try_stats();
      void try_deletes();

      // ### Type Definitions ###

      // ### Options ### //
      Worker &set_console(const bool onoff) {
        _console = onoff;
        return *this;
      } // set_console

      // ### StatsClient Pure Virtuals ### //
      void onDescribeStats();
      void onDestroyStats();

    protected:
      void try_stompstats();

    private:
      // constructor variables
      std::string _stomp_hosts;
      std::string _stomp_dest;
      std::string _stomp_login;
      std::string _stomp_passcode;
      std::string _memcached_host;
      std::string _db_host;
      std::string _db_user;
      std::string _db_pass;
      std::string _db_database;
      bool _drop_defer;

      Store *_store;
      stomp::Stomp *_stomp;

      openframe::Intval *_delete_intval;

      bool _connected;
      bool _console;

      struct aprs_stats_t {
        unsigned int packet;
        unsigned int raw;
      }; // aprs_stats_t

      struct obj_stats_t {
        unsigned int connects;
        unsigned int disconnects;
        unsigned int packets;
        unsigned int raw;
        unsigned int frames_in;
        unsigned int frames_out;
        time_t report_interval;
        time_t last_report_at;
        time_t created_at;
      } _stats;
      void init_stats(obj_stats_t &stats, const bool startup = false);

      struct obj_stompstats_t {
        aprs_stats_t aprs_stats;
        time_t report_interval;
        time_t last_report_at;
        time_t created_at;
      } _stompstats;
      void init_stompstats(obj_stompstats_t &stats, const bool startup = false);
  }; // class Worker

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace aprspruner
#endif
