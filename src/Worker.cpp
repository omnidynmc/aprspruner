#include "config.h"

#include <string>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <openframe/openframe.h>
#include <stomp/StompHeaders.h>
#include <stomp/StompFrame.h>
#include <stomp/Stomp.h>
#include <aprs/aprs.h>

#include <Worker.h>
#include <Store.h>
#include <MemcachedController.h>
#include <DBI.h>

namespace aprspruner {
  using namespace openframe::loglevel;

  const int Worker::kDefaultStompPrefetch	= 1024;
  const time_t Worker::kDefaultStatsInterval	= 3600;
  const time_t Worker::kDefaultDeleteInterval	= 10;
  const char *Worker::kStompDestErrors		= "/topic/feeds.aprs.is.errors";

  Worker::Worker(const openframe::LogObject::thread_id_t thread_id,
                 const std::string &stomp_hosts,
                 const std::string &stomp_dest,
                 const std::string &stomp_login,
                 const std::string &stomp_passcode,
                 const std::string &db_host,
                 const std::string &db_user,
                 const std::string &db_pass,
                 const std::string &db_database)
         : openframe::LogObject(thread_id),
           _stomp_hosts(stomp_hosts),
           _stomp_dest(stomp_dest),
           _stomp_login(stomp_login),
           _stomp_passcode(stomp_passcode),
           _db_host(db_host),
           _db_user(db_user),
           _db_pass(db_pass),
           _db_database(db_database) {

    _store = NULL;
    _stomp = NULL;
    _connected = false;
    _console = false;
    _delete_intval = new openframe::Intval(kDefaultDeleteInterval);

    init_stats(_stats, true);
    init_stompstats(_stompstats, true);
    _stats.report_interval = 60;
    _stompstats.report_interval = 5;
  } // Worker::Worker

  Worker::~Worker() {
    onDestroyStats();

    delete _delete_intval;
    if (_store) delete _store;
    if (_stomp) delete _stomp;
  } // Worker:~Worker

  void Worker::init() {
    try {
      stomp::StompHeaders *headers = new stomp::StompHeaders("openstomp.prefetch",
                                                             openframe::stringify<int>(kDefaultStompPrefetch)
                                                            );
      headers->add_header("heart-beat", "0,5000");
      _stomp = new stomp::Stomp(_stomp_hosts,
                                _stomp_login,
                                _stomp_passcode,
                                headers);

      _store = new Store(thread_id(),
                         _db_host,
                         _db_user,
                         _db_pass,
                         _db_database,
                         _memcached_host,
                         kDefaultStatsInterval);
      _store->replace_stats( stats(), "");
      _store->set_elogger( elogger(), elog_name() );
      _store->init();
    } // try
    catch(std::bad_alloc &xa) {
      assert(false);
    } // catch
  } // Worker::init

  void Worker::init_stats(obj_stats_t &stats, const bool startup) {
    stats.connects = 0;
    stats.disconnects = 0;
    stats.packets = 0;
    stats.frames_in = 0;
    stats.frames_out = 0;

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stats

  void Worker::init_stompstats(obj_stompstats_t &stats, const bool startup) {
    memset(&stats.aprs_stats, 0, sizeof(aprs_stats_t) );

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stompstats

  void Worker::onDescribeStats() {
    describe_stat("num.sql.inserted", "worker"+thread_id_str()+"/sql inserted", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.sql.failed", "worker"+thread_id_str()+"/sql failed", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("time.run", "worker"+thread_id_str()+"/run loop time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.sql.insert", "worker"+thread_id_str()+"/sql insert time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.parse.aprs", "worker"+thread_id_str()+"/parse aprs time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.write.event", "worker"+thread_id_str()+"/write event time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);

    // APRS Packet Stats
    describe_stat("aprs_stats.delete.packet", "aprs/stats/delete/packet", openstats::graphTypeCounter);
    describe_stat("aprs_stats.delete.raw", "aprs/stats/delete/raw", openstats::graphTypeCounter);
  } // Worker::onDescribeStats

  void Worker::onDestroyStats() {
    destroy_stat("*");
  } // Worker::onDestroyStats

  void Worker::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    int diff = time(NULL) - _stats.last_report_at;
    double ppm = double(_stompstats.aprs_stats.packet) / diff;
    double rpm = double(_stompstats.aprs_stats.raw) / diff;

    TLOG(LogNotice, << "Deletes packets " << _stompstats.aprs_stats.packet
                    << ", ppm " << ppm << "/m"
                    << ", raw " << _stompstats.aprs_stats.raw
                    << ", rpm " << rpm << "/m"
                    << ", next in " << _stats.report_interval
                    << ", connect attempts " << _stats.connects
                    << "; " << _stomp->connected_to()
                    << std::endl);

    init_stats(_stats);
    _stats.last_report_at = time(NULL);
  } // Worker::try_stats

  void Worker::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    datapoint("aprs_stats.delete.packet", _stompstats.aprs_stats.packet);
    datapoint("aprs_stats.delete.raw", _stompstats.aprs_stats.raw);

    init_stompstats(_stompstats);
  } // Worker::try_stompstats

  bool Worker::run() {
    try_stats();
    _store->try_stats();

    try_deletes();

    sleep(kDefaultDeleteInterval);

    return true;
  } // Worker::run

  void Worker::try_deletes() {
    if ( !_delete_intval->is_next() ) return;

    size_t num_packets_deleted = _store->deletePacketsByAge(86400 * 5, 2000);
    size_t num_raw_deleted = _store->deleteRawByAge(86400, 2000);

    _stompstats.aprs_stats.packet += num_packets_deleted;
    _stompstats.aprs_stats.raw += num_raw_deleted;

    TLOG(LogNotice, << "Delete packets "
                    << num_packets_deleted
                    << ", raw "
                    << num_raw_deleted
                    << std::endl);

  } // worker::try_locators
} // namespace aprsoruner
