#include "multilock.h"
#include <time.h>

namespace pintool {

using namespace race;

MultiLock::Meta *MultiLock::GetMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new MultiLockMeta(iaddr);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void MultiLock::AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                                 Inst *inst, thread_id_t child_thd_id) {
	Detector::AfterPthreadJoin(curr_thd_id, curr_thd_clk, inst, child_thd_id);
	VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	// increment the vector clock
	curr_vc->Increment(curr_thd_id);
}

void MultiLock::ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
	lockset_map_[curr_thd_id].Add(addr);
}

void MultiLock::ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
	lockset_map_[curr_thd_id].Remove(addr);
}

inline bool MultiLock::UpdateOnRead(thread_id_t curr_thd_id, MultiLockMeta *multilock_meta,
		                     VectorClock *curr_vc, Lockset &curr_ls, Inst *inst) {
	update_r_++;
	bool is_redundant = false;
	MultiLockMeta::ELPSet &reader_els = multilock_meta->reader_els_map[curr_thd_id];
	MultiLockMeta::ELPSet &writer_els = multilock_meta->writer_els_map[curr_thd_id];
	MultiLockMeta::InstMap &reader_inst_table = multilock_meta->reader_inst_table;

	for (MultiLockMeta::ELPSet::iterator it = reader_els.begin(); it != reader_els.end(); ++it) {
		const Epoch &reader_epoch = it->first;
		const Lockset &reader_lockset = it->second;
		if (reader_epoch.Equal(curr_vc) && !reader_lockset.IsSubsetOf(curr_ls)) {
			is_redundant = true;
			return is_redundant;
		}
	}

	for (MultiLockMeta::ELPSet::iterator it = writer_els.begin(); it != writer_els.end(); ++it) {
		const Epoch &writer_epoch = it->first;
		const Lockset &writer_lockset = it->second;
		if (writer_epoch.Equal(curr_vc) && !writer_lockset.IsSubsetOf(curr_ls)) {
			is_redundant = true;
			return is_redundant;
		}
	}

	for (MultiLockMeta::ELPSet::iterator it = reader_els.begin(); it != reader_els.end(); ++it) {
		const Epoch &reader_epoch = it->first;
		const Lockset &reader_lockset = it->second;
		if (reader_epoch.Equal(curr_vc) && reader_lockset.IsSupersetOf(curr_ls)) {
			reader_els.erase(*it);
		    reader_inst_table.erase(*it);
		    eli_r_c_++;
		}
	}

	Epoch curr_epoch;
	curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
	MultiLockMeta::ELP elp(curr_epoch, curr_ls);
	reader_els.insert(elp);
	reader_inst_table[elp] = inst;
	return is_redundant;
}

inline bool MultiLock::UpdateOnWrite(thread_id_t curr_thd_id, MultiLockMeta *multilock_meta,
                              VectorClock *curr_vc, Lockset &curr_ls, Inst *inst) {
	update_w_++;
	bool is_redundant = false;
	MultiLockMeta::ELPSet &reader_els = multilock_meta->reader_els_map[curr_thd_id];
	MultiLockMeta::ELPSet &writer_els = multilock_meta->writer_els_map[curr_thd_id];
	MultiLockMeta::InstMap &reader_inst_table = multilock_meta->reader_inst_table;
	MultiLockMeta::InstMap &writer_inst_table = multilock_meta->writer_inst_table;

	for (MultiLockMeta::ELPSet::iterator it = writer_els.begin(); it != writer_els.end(); ++it) {
		const Epoch &writer_epoch = it->first;
		const Lockset &writer_lockset = it->second;
		if (writer_epoch.Equal(curr_vc) && writer_lockset.IsSubsetOf(curr_ls)) {
			is_redundant = true;
			return is_redundant;
		}
	}

	for (MultiLockMeta::ELPSet::iterator it = reader_els.begin(); it != reader_els.end(); ++it) {
		const Epoch &reader_epoch = it->first;
		const Lockset &reader_lockset = it->second;
		if (reader_epoch.Equal(curr_vc) && reader_lockset.IsSupersetOf(curr_ls)) {
			reader_els.erase(*it);
		    reader_inst_table.erase(*it);
		    eli_r_c_++;
		}
	}

	for (MultiLockMeta::ELPSet::iterator it = writer_els.begin(); it != writer_els.end(); ++it) {
		const Epoch &writer_epoch = it->first;
		const Lockset &writer_lockset = it->second;
		if (writer_epoch.Equal(curr_vc) && writer_lockset.IsSupersetOf(curr_ls)) {
			writer_els.erase(*it);
		    writer_inst_table.erase(*it);
		    eli_w_c_++;
		}
	}

	Epoch curr_epoch;
	curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
	MultiLockMeta::ELP elp(curr_epoch, curr_ls);
	writer_els.insert(elp);
	writer_inst_table[elp] = inst;
	return is_redundant;
}

void MultiLock::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  MultiLockMeta *multilock_meta = dynamic_cast<MultiLockMeta *>(meta);
  DEBUG_ASSERT(multilock_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  MultiLockMeta::ELSMap &writer_els_map = multilock_meta->writer_els_map;
  clock_t t = clock();
  bool is_redundant = UpdateOnRead(curr_thd_id, multilock_meta, curr_vc, curr_ls, inst);
  t = clock() - t;
  time_eli_read += ((float)t)/CLOCKS_PER_SEC;
  // skip redundant
  if (is_redundant) {
	  return;
  }
  // check writers
  for (MultiLockMeta::ELSMap::iterator mit = writer_els_map.begin(); mit != writer_els_map.end(); ++mit) {
    if (mit->first == curr_thd_id) continue;
	for (MultiLockMeta::ELPSet::iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
		detect_wr_++;
		const Epoch &writer_epoch = it->first;
		const Lockset &writer_lockset = it->second;
		if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
			DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
			DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", multilock_meta->addr);
			DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
			// mark the meta as racy
			multilock_meta->racy = true;
			// RAW race detected, report them
			thread_id_t thd_id = writer_epoch.GetTid();
			timestamp_t clk = writer_epoch.GetClock();
			if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
		      DEBUG_ASSERT(multilock_meta->writer_inst_table.find(*it) !=
		    		  multilock_meta->writer_inst_table.end());
			  Inst *writer_inst = multilock_meta->writer_inst_table[*it];
			  // report the race
			  ReportRace(multilock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
			               curr_thd_id, inst, RACE_EVENT_READ);
			}
		}
	}
  }
  // update race inst set if needed
  if (track_racy_inst_) {
    multilock_meta->race_inst_set.insert(inst);
  }
}

void MultiLock::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  MultiLockMeta *multilock_meta = dynamic_cast<MultiLockMeta *>(meta);
  DEBUG_ASSERT(multilock_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  MultiLockMeta::ELSMap &reader_els_map = multilock_meta->reader_els_map;
  MultiLockMeta::ELSMap &writer_els_map = multilock_meta->writer_els_map;
  clock_t t = clock();
  bool is_redundant = UpdateOnWrite(curr_thd_id, multilock_meta, curr_vc, curr_ls, inst);
  t = clock() - t;
  time_eli_write += ((float)t)/CLOCKS_PER_SEC;
  // skip redundant
  if (is_redundant) {
	  return;
  }
  // check writers
  for (MultiLockMeta::ELSMap::iterator mit = writer_els_map.begin(); mit != writer_els_map.end(); ++mit) {
	  if (mit->first == curr_thd_id) continue;
	  for (MultiLockMeta::ELPSet::iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
		  detect_ww_++;
		  const Epoch &writer_epoch = it->first;
		const Lockset &writer_lockset = it->second;
		if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
			DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
			DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", multilock_meta->addr);
			DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
			// mark the meta as racy
			multilock_meta->racy = true;
			// WAW race detected, report them
			thread_id_t thd_id = writer_epoch.GetTid();
			timestamp_t clk = writer_epoch.GetClock();
			if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
			  DEBUG_ASSERT(multilock_meta->writer_inst_table.find(*it) !=
			    		  multilock_meta->writer_inst_table.end());
			  Inst *writer_inst = multilock_meta->writer_inst_table[*it];
			  // report the race
			  ReportRace(multilock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
			               curr_thd_id, inst, RACE_EVENT_WRITE);
			}
		}
	}
  }
  // check readers
  for (MultiLockMeta::ELSMap::iterator mit = reader_els_map.begin(); mit != reader_els_map.end(); ++mit) {
	  if (mit->first == curr_thd_id) continue;
	  for (MultiLockMeta::ELPSet::iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
		  detect_rw_++;
		  const Epoch &reader_epoch = it->first;
		const Lockset &reader_lockset = it->second;
		if (!reader_epoch.HappensBefore(curr_vc) && reader_lockset.IsDisjoint(curr_ls)) {
			DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
			DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", multilock_meta->addr);
			DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
			// mark the meta as racy
			multilock_meta->racy = true;
			// WAR race detected, report them
			thread_id_t thd_id = reader_epoch.GetTid();
			timestamp_t clk = reader_epoch.GetClock();
			if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
			  DEBUG_ASSERT(multilock_meta->reader_inst_table.find(*it) !=
			    		  multilock_meta->reader_inst_table.end());
			  Inst *reader_inst = multilock_meta->reader_inst_table[*it];
			  // report the race
			  ReportRace(multilock_meta, thd_id, reader_inst, RACE_EVENT_READ,
			               curr_thd_id, inst, RACE_EVENT_WRITE);
			}
		}
	}
  }
  // update race inst set if needed
  if (track_racy_inst_) {
    multilock_meta->race_inst_set.insert(inst);
  }
}

void MultiLock::ProcessFree(Meta *meta) {
  // cast the meta
  MultiLockMeta *multilock_meta = dynamic_cast<MultiLockMeta *>(meta);
  DEBUG_ASSERT(multilock_meta);
  // update racy inst set if needed
  if (track_racy_inst_ && multilock_meta->racy) {
    for (MultiLockMeta::InstSet::iterator it = multilock_meta->race_inst_set.begin();
         it != multilock_meta->race_inst_set.end(); ++it) {
      race_db_->SetRacyInst(*it, true);
    }
  }
  delete multilock_meta;
}

}
