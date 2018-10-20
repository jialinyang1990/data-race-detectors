#include "acculock.h"

namespace pintool {

using namespace race;

AccuLock::Meta *AccuLock::GetMeta(address_t iaddr) {
	Meta::Table::iterator it = meta_table_.find(iaddr);
	if (it == meta_table_.end()) {
		Meta *meta = new AccuLockMeta(iaddr);
		meta_table_[iaddr] = meta;
		return meta;
	} else {
		return it->second;
	}
}

void AccuLock::ProcessLock(thread_id_t curr_thd_id, address_t addr,
		MutexMeta *meta) {
	lockset_map_[curr_thd_id].Add(addr);
}

void AccuLock::ProcessUnlock(thread_id_t curr_thd_id, address_t addr,
		MutexMeta *meta) {
	lockset_map_[curr_thd_id].Remove(addr);
	VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	// increment the vector clock
	curr_vc->Increment(curr_thd_id);
}

void AccuLock::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
	// cast the meta
	AccuLockMeta *acculock_meta = dynamic_cast<AccuLockMeta *>(meta);
	DEBUG_ASSERT(acculock_meta);
	// get the current vector clock
	VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
	Lockset &curr_ls = lockset_map_[curr_thd_id];
	Epoch &writer_epoch = acculock_meta->writer_elp.first;
	Lockset &writer_lockset = acculock_meta->writer_elp.second;
	Epoch &reader_epoch = acculock_meta->reader_elp_map[curr_thd_id].first;
	Lockset &reader_lockset = acculock_meta->reader_elp_map[curr_thd_id].second;
	// skip same epoch
	if (reader_epoch.Equal(curr_vc) || writer_epoch.Equal(curr_vc)) {
		skip_r_c_++;
		return;
	}
	// check writers
	detect_wr_++;
	if (!writer_epoch.HappensBefore(curr_vc)
			&& writer_lockset.IsDisjoint(curr_ls)) {
		DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
		DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", acculock_meta->addr);
		DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
		// mark the meta as racy
		acculock_meta->racy = true;
		// RAW race detected, report them
		thread_id_t thd_id = writer_epoch.GetTid();
		timestamp_t clk = writer_epoch.GetClock();
		if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
			DEBUG_ASSERT(acculock_meta->writer_inst != NULL);
			Inst *writer_inst = acculock_meta->writer_inst;
			// report the race
			ReportRace(acculock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
					curr_thd_id, inst, RACE_EVENT_READ);
		}
	}
	// update meta data
	update_r_++;
	reader_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
	reader_lockset = curr_ls;
	acculock_meta->reader_inst_table[curr_thd_id] = inst;
	// update race inst set if needed
	if (track_racy_inst_) {
		acculock_meta->race_inst_set.insert(inst);
	}
}

void AccuLock::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
	// cast the meta
	AccuLockMeta *acculock_meta = dynamic_cast<AccuLockMeta *>(meta);
	DEBUG_ASSERT(acculock_meta);
	// get the current vector clock
	VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
	Lockset &curr_ls = lockset_map_[curr_thd_id];
	Epoch &writer_epoch = acculock_meta->writer_elp.first;
	Lockset &writer_lockset = acculock_meta->writer_elp.second;
	AccuLockMeta::ELPMap &reader_elp_map = acculock_meta->reader_elp_map;
	// skip same epoch
	if (writer_epoch.Equal(curr_vc)) {
		skip_w_c_++;
		return;
	}
	// check writers
	detect_ww_++;
	if (!writer_epoch.HappensBefore(curr_vc)
			&& writer_lockset.IsDisjoint(curr_ls)) {
		DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
		DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", acculock_meta->addr);
		DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
		// mark the meta as racy
		acculock_meta->racy = true;
		// WAW race detected, report them
		thread_id_t thd_id = writer_epoch.GetTid();
		timestamp_t clk = writer_epoch.GetClock();
		if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
			DEBUG_ASSERT(acculock_meta->writer_inst != NULL);
			Inst *writer_inst = acculock_meta->writer_inst;
			// report the race
			ReportRace(acculock_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
					curr_thd_id, inst, RACE_EVENT_WRITE);
		}
	}
	// check readers
	for (AccuLockMeta::ELPMap::iterator it = reader_elp_map.begin();
			it != reader_elp_map.end(); ++it) {
		detect_rw_++;
		Epoch &reader_epoch = it->second.first;
		Lockset &reader_lockset = it->second.second;
		if (!reader_epoch.HappensBefore(curr_vc)
				&& reader_lockset.IsDisjoint(curr_ls)) {
			DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
			DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", acculock_meta->addr);
			DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
			// mark the meta as racy
			acculock_meta->racy = true;
			// WAR race detected, report them
			thread_id_t thd_id = reader_epoch.GetTid();
			timestamp_t clk = reader_epoch.GetClock();
			if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
				DEBUG_ASSERT(
						acculock_meta->reader_inst_table.find(thd_id) != acculock_meta->reader_inst_table.end());
				Inst *reader_inst = acculock_meta->reader_inst_table[thd_id];
				// report the race
				ReportRace(acculock_meta, thd_id, reader_inst, RACE_EVENT_READ,
						curr_thd_id, inst, RACE_EVENT_WRITE);
			}
		}
	}
	reader_elp_map.clear();
	// update meta data
	update_w_++;
	writer_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
	writer_lockset = curr_ls;
	acculock_meta->writer_inst = inst;
	// update race inst set if needed
	if (track_racy_inst_) {
		acculock_meta->race_inst_set.insert(inst);
	}
}

void AccuLock::ProcessFree(Meta *meta) {
	// cast the meta
	AccuLockMeta *acculock_meta = dynamic_cast<AccuLockMeta *>(meta);
	DEBUG_ASSERT(acculock_meta);
	// update racy inst set if needed
	if (track_racy_inst_ && acculock_meta->racy) {
		for (AccuLockMeta::InstSet::iterator it =
				acculock_meta->race_inst_set.begin();
				it != acculock_meta->race_inst_set.end(); ++it) {
			race_db_->SetRacyInst(*it, true);
		}
	}
	delete acculock_meta;
}

}
