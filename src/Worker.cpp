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
  const time_t Worker::kDefaultDeleteInterval	= 1;
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
    describe_stat("num.frames.out", "worker"+thread_id_str()+"/num frames out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.frames.in", "worker"+thread_id_str()+"/num frames in", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.out", "worker"+thread_id_str()+"/num bytes out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.in", "worker"+thread_id_str()+"/num bytes in", openstats::graphTypeCounter, openstats::dataTypeInt);

    describe_stat("num.work.in", "worker"+thread_id_str()+"/num work in", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.work.out", "worker"+thread_id_str()+"/work out", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.result.queue", "worker"+thread_id_str()+"/num result queue", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.aprs.rejects", "worker"+thread_id_str()+"/aprs rejects", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.aprs.duplicates", "worker"+thread_id_str()+"/aprs duplicates", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.aprs.position.error", "worker"+thread_id_str()+"/aprs position errors", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.aprs.deferred", "worker"+thread_id_str()+"/aprs deferred", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.aprs.errors", "worker"+thread_id_str()+"/aprs errors", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.sql.inserted", "worker"+thread_id_str()+"/sql inserted", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("num.sql.failed", "worker"+thread_id_str()+"/sql failed", openstats::graphTypeGauge, openstats::dataTypeInt, openstats::useTypeSum);
    describe_stat("time.run", "worker"+thread_id_str()+"/run loop time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.sql.insert", "worker"+thread_id_str()+"/sql insert time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.parse.aprs", "worker"+thread_id_str()+"/parse aprs time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);
    describe_stat("time.write.event", "worker"+thread_id_str()+"/write event time", openstats::graphTypeGauge, openstats::dataTypeFloat, openstats::useTypeMean);

    // APRS Packet Stats
    describe_stat("aprs_stats.rate.packet", "aprs stats/rate/packet", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.position", "aprs stats/rate/positions", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.message", "aprs stats/rate/message", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.telemetry", "aprs stats/rate/telemetry", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.status", "aprs stats/rate/status", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.capabilities", "aprs stats/rate/capabilities", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.peet_logging", "aprs stats/rate/peet_logging", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.weather", "aprs stats/rate/weather", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.dx", "aprs stats/rate/dx", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.experimental", "aprs stats/rate/experimental", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.beacon", "aprs stats/rate/beacon", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.unknown", "aprs stats/rate/unknown", openstats::graphTypeCounter);

    describe_stat("aprs_stats.rate.reject.invparse", "aprs stats/rate/reject/invparse", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.reject.duplicate", "aprs stats/rate/reject/duplicate", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.reject.tofast", "aprs stats/rate/reject/tofast", openstats::graphTypeCounter);
    describe_stat("aprs_stats.rate.reject.tosoon", "aprs stats/rate/reject/tosoon", openstats::graphTypeCounter);

    describe_stat("aprs_stats.ratio.position", "aprs stats/ratio/positions", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.message", "aprs stats/ratio/message", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.telemetry", "aprs stats/ratio/telemetry", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.status", "aprs stats/ratio/status", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.capabilities", "aprs stats/ratio/capabilities", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.peet_logging", "aprs stats/ratio/peet_logging", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.weather", "aprs stats/ratio/weather", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.dx", "aprs stats/ratio/dx", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.experimental", "aprs stats/ratio/experimental", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.beacon", "aprs stats/ratio/beacon", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.unknown", "aprs stats/ratio/unknown", openstats::graphTypeGauge, openstats::dataTypeFloat);

    describe_stat("aprs_stats.ratio.reject.invparse", "aprs stats/ratio/reject/invparse", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.reject.duplicate", "aprs stats/ratio/reject/duplicate", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.reject.tosoon", "aprs stats/ratio/reject/too soon", openstats::graphTypeGauge, openstats::dataTypeFloat);
    describe_stat("aprs_stats.ratio.reject.tofast", "aprs stats/ratio/reject/too fast", openstats::graphTypeGauge, openstats::dataTypeFloat);
  } // Worker::onDescribeStats

  void Worker::onDestroyStats() {
    destroy_stat("*");
  } // Worker::onDestroyStats

  void Worker::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    int diff = time(NULL) - _stats.last_report_at;
    double pps = double(_stats.packets) / diff;
    double fps_in = double(_stats.frames_in) / diff;
    double fps_out = double(_stats.frames_out) / diff;

    TLOG(LogNotice, << "Stats packets " << _stats.packets
                    << ", pps " << pps << "/s"
                    << ", frames in " << _stats.frames_in
                    << ", fps in " << fps_in << "/s"
                    << ", frames out " << _stats.frames_out
                    << ", fps out " << fps_out << "/s"
                    << ", next in " << _stats.report_interval
                    << ", connect attempts " << _stats.connects
                    << "; " << _stomp->connected_to()
                    << std::endl);

    init_stats(_stats);
    _stats.last_report_at = time(NULL);
  } // Worker::try_stats

  void Worker::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    datapoint("aprs_stats.rate.packet", _stompstats.aprs_stats.packet);
    datapoint("aprs_stats.rate.position", _stompstats.aprs_stats.position);
    datapoint("aprs_stats.rate.message", _stompstats.aprs_stats.message);
    datapoint("aprs_stats.rate.telemetry", _stompstats.aprs_stats.telemetry);
    datapoint("aprs_stats.rate.status", _stompstats.aprs_stats.status);
    datapoint("aprs_stats.rate.capabilities", _stompstats.aprs_stats.capabilities);
    datapoint("aprs_stats.rate.peet_logging", _stompstats.aprs_stats.peet_logging);
    datapoint("aprs_stats.rate.weather", _stompstats.aprs_stats.weather);
    datapoint("aprs_stats.rate.dx", _stompstats.aprs_stats.dx);
    datapoint("aprs_stats.rate.experimental", _stompstats.aprs_stats.experimental);
    datapoint("aprs_stats.rate.beacon", _stompstats.aprs_stats.beacon);
    datapoint("aprs_stats.rate.unknown", _stompstats.aprs_stats.unknown);

    datapoint("aprs_stats.rate.reject.invparse", _stompstats.aprs_stats.reject_invparse);
    datapoint("aprs_stats.rate.reject.duplicate", _stompstats.aprs_stats.reject_duplicate);
    datapoint("aprs_stats.rate.reject.tofast", _stompstats.aprs_stats.reject_tofast);
    datapoint("aprs_stats.rate.reject.tosoon", _stompstats.aprs_stats.reject_tosoon);

    datapoint_float("aprs_stats.ratio.position", OPENSTATS_PERCENT(_stompstats.aprs_stats.position, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.message", OPENSTATS_PERCENT(_stompstats.aprs_stats.message, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.telemetry", OPENSTATS_PERCENT(_stompstats.aprs_stats.telemetry, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.status", OPENSTATS_PERCENT(_stompstats.aprs_stats.status, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.capabilities", OPENSTATS_PERCENT(_stompstats.aprs_stats.capabilities, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.peet_logging", OPENSTATS_PERCENT(_stompstats.aprs_stats.peet_logging, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.weather", OPENSTATS_PERCENT(_stompstats.aprs_stats.weather, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.dx", OPENSTATS_PERCENT(_stompstats.aprs_stats.dx, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.experimental", OPENSTATS_PERCENT(_stompstats.aprs_stats.experimental, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.beacon", OPENSTATS_PERCENT(_stompstats.aprs_stats.beacon, _stompstats.aprs_stats.packet) );

    datapoint_float("aprs_stats.ratio.reject.invparse", OPENSTATS_PERCENT(_stompstats.aprs_stats.reject_invparse, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.reject.duplicate", OPENSTATS_PERCENT(_stompstats.aprs_stats.reject_duplicate, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.reject.tofast", OPENSTATS_PERCENT(_stompstats.aprs_stats.reject_tofast, _stompstats.aprs_stats.packet) );
    datapoint_float("aprs_stats.ratio.reject.tosoon", OPENSTATS_PERCENT(_stompstats.aprs_stats.reject_tosoon, _stompstats.aprs_stats.packet) );

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

    size_t num_packets_deleted = _store->deletePacketsByAge(3600, 2000);
    size_t num_raw_deleted = _store->deleteRawByAge(3600, 2000);

    TLOG(LogNotice, << "Delete packets "
                    << num_packets_deleted
                    << ", raw "
                    << num_raw_deleted
                    << std::endl);

  } // worker::try_locators
} // namespace aprsoruner
