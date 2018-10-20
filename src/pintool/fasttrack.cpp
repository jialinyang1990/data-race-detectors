#include "fasttrack.h"

namespace pintool {

using namespace race;

FastTrack::Meta *FastTrack::GetMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new FastTrackMeta(iaddr);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void FastTrack::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  FastTrackMeta *fasttrack_meta = dynamic_cast<FastTrackMeta *>(meta);
  DEBUG_ASSERT(fasttrack_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Epoch &writer_epoch = fasttrack_meta->writer_epoch;
  Epoch &reader_epoch = fasttrack_meta->reader_epoch;
  VectorClock &reader_vc = fasttrack_meta->reader_vc;
  // skip redundant
  if (reader_epoch.Equal(curr_vc)) {
	  skip_r_c_++;
	  return;
  }
  // check writers
  if (!writer_epoch.HappensBefore(curr_vc)) {
    detect_wr_++;
	  DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", fasttrack_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    fasttrack_meta->racy = true;
    // RAW race detected, report them
    thread_id_t thd_id = writer_epoch.GetTid();
    timestamp_t clk = writer_epoch.GetClock();
    if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
      DEBUG_ASSERT(fasttrack_meta->writer_inst != NULL);
      Inst *writer_inst = fasttrack_meta->writer_inst;
      // report the race
      ReportRace(fasttrack_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                 curr_thd_id, inst, RACE_EVENT_READ);
    }
  }
  // update meta data
  update_r_++;
  if(!reader_epoch.HappensBefore(curr_vc)) {
	  fasttrack_meta->read_shared = true;
  }
  reader_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  reader_vc.SetClock(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  fasttrack_meta->reader_inst_table[curr_thd_id] = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    fasttrack_meta->race_inst_set.insert(inst);
  }
}

void FastTrack::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  FastTrackMeta *fasttrack_meta = dynamic_cast<FastTrackMeta *>(meta);
  DEBUG_ASSERT(fasttrack_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Epoch &writer_epoch = fasttrack_meta->writer_epoch;
  Epoch &reader_epoch = fasttrack_meta->reader_epoch;
  VectorClock &reader_vc = fasttrack_meta->reader_vc;
  // skip redundant
  if (!writer_epoch.IsInitial() && writer_epoch.Equal(curr_vc)) {
	  skip_w_c_++;
	  return;
  }
  // check writers
  if (!writer_epoch.HappensBefore(curr_vc)) {
    detect_ww_++;
	  DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", fasttrack_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    fasttrack_meta->racy = true;
    // WAW race detected, report them
    thread_id_t thd_id = writer_epoch.GetTid();
    timestamp_t clk = writer_epoch.GetClock();
    if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
      DEBUG_ASSERT(fasttrack_meta->writer_inst != NULL);
      Inst *writer_inst = fasttrack_meta->writer_inst;
      // report the race
      ReportRace(fasttrack_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                 curr_thd_id, inst, RACE_EVENT_WRITE);
    }
  }
  // check readers
  if (!fasttrack_meta->read_shared && !reader_epoch.HappensBefore(curr_vc)) {
	detect_rw_++;
	  DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
	DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", fasttrack_meta->addr);
	DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
	// mark the meta as racy
	fasttrack_meta->racy = true;
    // WAR race detected, report them
	thread_id_t thd_id = reader_epoch.GetTid();
	timestamp_t clk = reader_epoch.GetClock();
	if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
	  DEBUG_ASSERT(fasttrack_meta->reader_inst_table.find(thd_id) !=
	               fasttrack_meta->reader_inst_table.end());
	  Inst *reader_inst = fasttrack_meta->reader_inst_table[thd_id];
	  // report the race
	  ReportRace(fasttrack_meta, thd_id, reader_inst, RACE_EVENT_READ,
	             curr_thd_id, inst, RACE_EVENT_WRITE);
    }
  } else if (fasttrack_meta->read_shared && !reader_vc.HappensBefore(curr_vc)) {
		detect_rw_++;
	  DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", fasttrack_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    fasttrack_meta->racy = true;
    // WAR race detected, report them
    for (reader_vc.IterBegin(); !reader_vc.IterEnd(); reader_vc.IterNext()) {
      thread_id_t thd_id = reader_vc.IterCurrThd();
      timestamp_t clk = reader_vc.IterCurrClk();
      if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
        DEBUG_ASSERT(fasttrack_meta->reader_inst_table.find(thd_id) !=
                     fasttrack_meta->reader_inst_table.end());
        Inst *reader_inst = fasttrack_meta->reader_inst_table[thd_id];
        // report the race
        ReportRace(fasttrack_meta, thd_id, reader_inst, RACE_EVENT_READ,
                   curr_thd_id, inst, RACE_EVENT_WRITE);
      }
    }
    fasttrack_meta->read_shared = false;
  }
  // update meta data
  update_r_++;
  writer_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  fasttrack_meta->writer_inst = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    fasttrack_meta->race_inst_set.insert(inst);
  }
}

void FastTrack::ProcessFree(Meta *meta) {
  // cast the meta
  FastTrackMeta *fasttrack_meta = dynamic_cast<FastTrackMeta *>(meta);
  DEBUG_ASSERT(fasttrack_meta);
  // update racy inst set if needed
  if (track_racy_inst_ && fasttrack_meta->racy) {
    for (FastTrackMeta::InstSet::iterator it = fasttrack_meta->race_inst_set.begin();
         it != fasttrack_meta->race_inst_set.end(); ++it) {
      race_db_->SetRacyInst(*it, true);
    }
  }
  delete fasttrack_meta;
}

}
