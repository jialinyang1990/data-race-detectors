#define WINDOW_SIZE 100

#include "histlock.h"

namespace pintool {

using namespace race;

HistLock::Meta *HistLock::GetMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new HistLockMeta(iaddr);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void HistLock::BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t target) {
	callsite_map_[curr_thd_id] = target;
}

void HistLock::ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
  lockset_map_[curr_thd_id].Add(addr);
}

void HistLock::ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
  lockset_map_[curr_thd_id].Remove(addr);
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  // increment the vector clock
  curr_vc->Increment(curr_thd_id);
}

void HistLock::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  HistLockMeta *histlock_meta = dynamic_cast<HistLockMeta *>(meta);
  DEBUG_ASSERT(histlock_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  HistLockMeta::ELPIList &reader_elpi_list = histlock_meta->reader_elpi_list;
  HistLockMeta::ELPIList &writer_elpi_list = histlock_meta->writer_elpi_list;
  HistLockMeta::CallSiteELPIList &reader_callsite = histlock_meta->reader_callsite_elpi_map[curr_thd_id];

  if (!reader_elpi_list.empty()) {
	  HistLockMeta::ELPI &last_r = histlock_meta->reader_elpi_list.back();
	  if (last_r.first.first.Equal(curr_vc)) {
		  skip_r_c_++;
		  return;
	  }
  }
  if (!writer_elpi_list.empty()) {
	  HistLockMeta::ELPI &last_w = histlock_meta->writer_elpi_list.back();
	  if (last_w.first.first.Equal(curr_vc)) {
		  skip_r_c_++;
		  return;
	  }
  }

  // check writers
  for (HistLockMeta::ELPIList::iterator it = writer_elpi_list.begin(); it != writer_elpi_list.end(); ++it) {
	  detect_wr_++;
	  Epoch &writer_epoch = it->first.first;
	  Lockset &writer_lockset = it->first.second;
	  if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
	    DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
	    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlock_meta->addr);
	    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
	    // mark the meta as racy
	    histlock_meta->racy = true;
	    // RAW race detected, report them
	    thread_id_t thd_id = writer_epoch.GetTid();
	    timestamp_t clk = writer_epoch.GetClock();
	    if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
	      DEBUG_ASSERT(it->second != NULL);
	      Inst *writer_inst = it->second;
	      // report the race
	      ReportRace(histlock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
	                 curr_thd_id, inst, RACE_EVENT_READ);
	    }
	  }
  }

  clock_t t = clock();
  if(reader_callsite.first != callsite_map_[curr_thd_id]) {
	  for (HistLockMeta::ELPIIterList::iterator it = reader_callsite.second.begin(); it != reader_callsite.second.end(); ++it) {
		  eli_r_c_++;
		  reader_elpi_list.erase(*it);
	  }
	  reader_callsite.first = callsite_map_[curr_thd_id];
	  reader_callsite.second.clear();
  }
  t = clock() - t;
  time_eli_read += ((float)t)/CLOCKS_PER_SEC;

  // update meta data
  update_r_++;
  Epoch curr_epoch;
  curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  HistLockMeta::ELP elp(curr_epoch, curr_ls);
  HistLockMeta::ELPI elpi(elp, inst);
  HistLockMeta::ELPIList::iterator position = reader_elpi_list.insert(reader_elpi_list.end(),elpi);
  histlock_meta->reader_callsite_elpi_map[curr_thd_id].second.push_back(position);

  if (reader_elpi_list.size() > WINDOW_SIZE) {
	  reader_elpi_list.pop_front();
  }

  // update race inst set if needed
  if (track_racy_inst_) {
    histlock_meta->race_inst_set.insert(inst);
  }
}

void HistLock::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  HistLockMeta *histlock_meta = dynamic_cast<HistLockMeta *>(meta);
  DEBUG_ASSERT(histlock_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  HistLockMeta::ELPIList &reader_elpi_list = histlock_meta->reader_elpi_list;
  HistLockMeta::ELPIList &writer_elpi_list = histlock_meta->writer_elpi_list;
  HistLockMeta::CallSiteELPIList &writer_callsite = histlock_meta->writer_callsite_elpi_map[curr_thd_id];

  if (!writer_elpi_list.empty()) {
	  HistLockMeta::ELPI &last_w = histlock_meta->writer_elpi_list.back();
	  if (last_w.first.first.Equal(curr_vc)) {
		  skip_r_c_++;
		  return;
	  }
  }

  // check writers
  for (HistLockMeta::ELPIList::iterator it = writer_elpi_list.begin(); it != writer_elpi_list.end(); ++it) {
	  detect_ww_++;
	  Epoch &writer_epoch = it->first.first;
	  Lockset &writer_lockset = it->first.second;
	  if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
	    DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
	    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlock_meta->addr);
	    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
	    // mark the meta as racy
	    histlock_meta->racy = true;
	    // WAW race detected, report them
	    thread_id_t thd_id = writer_epoch.GetTid();
	    timestamp_t clk = writer_epoch.GetClock();
	    if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
	      DEBUG_ASSERT(it->second != NULL);
	      Inst *writer_inst = it->second;
	      // report the race
	      ReportRace(histlock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
	                 curr_thd_id, inst, RACE_EVENT_WRITE);
	    }
	  }
  }
  // check readers
  for (HistLockMeta::ELPIList::iterator it = reader_elpi_list.begin(); it != reader_elpi_list.end(); ++it) {
	  detect_rw_++;
	  Epoch &reader_epoch = it->first.first;
	  Lockset &reader_lockset = it->first.second;
	  if (!reader_epoch.HappensBefore(curr_vc) && reader_lockset.IsDisjoint(curr_ls)) {
	    DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
	    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlock_meta->addr);
	    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
	    // mark the meta as racy
	    histlock_meta->racy = true;
	    // WAR race detected, report them
	    thread_id_t thd_id = reader_epoch.GetTid();
	    timestamp_t clk = reader_epoch.GetClock();
	    if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
	      DEBUG_ASSERT(it->second != NULL);
	      Inst *reader_inst = it->second;
	      // report the race
	      ReportRace(histlock_meta, thd_id, reader_inst, RACE_EVENT_READ,
	                 curr_thd_id, inst, RACE_EVENT_WRITE);
	    }
	  }
  }

  clock_t t = clock();
  if(writer_callsite.first != callsite_map_[curr_thd_id]) {
	  for (HistLockMeta::ELPIIterList::iterator it = writer_callsite.second.begin(); it != writer_callsite.second.end(); ++it) {
		  eli_w_c_++;
		  writer_elpi_list.erase(*it);
	  }
	  writer_callsite.first = callsite_map_[curr_thd_id];
	  writer_callsite.second.clear();
  }
  t = clock() - t;
  time_eli_write += ((float)t)/CLOCKS_PER_SEC;

  // update meta data
  update_w_++;
  Epoch curr_epoch;
  curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  HistLockMeta::ELP elp(curr_epoch, curr_ls);
  HistLockMeta::ELPI elpi(elp, inst);
  HistLockMeta::ELPIList::iterator position = writer_elpi_list.insert(writer_elpi_list.end(), elpi);
  histlock_meta->writer_callsite_elpi_map[curr_thd_id].second.push_back(position);

  if (writer_elpi_list.size() > WINDOW_SIZE) {
 	  writer_elpi_list.pop_front();
  }

  // update race inst set if needed
  if (track_racy_inst_) {
    histlock_meta->race_inst_set.insert(inst);
  }
}

void HistLock::ProcessFree(Meta *meta) {
  // cast the meta
  HistLockMeta *histlock_meta = dynamic_cast<HistLockMeta *>(meta);
  DEBUG_ASSERT(histlock_meta);
  // update racy inst set if needed
  if (track_racy_inst_ && histlock_meta->racy) {
    for (HistLockMeta::InstSet::iterator it = histlock_meta->race_inst_set.begin();
         it != histlock_meta->race_inst_set.end(); ++it) {
      race_db_->SetRacyInst(*it, true);
    }
  }
  delete histlock_meta;
}

}
