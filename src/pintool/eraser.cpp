#include "eraser.h"

namespace pintool {

using namespace race;

Eraser::Meta *Eraser::GetMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new EraserMeta(iaddr);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void Eraser::ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
  lockset_map_[curr_thd_id].Add(addr);
}

void Eraser::ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
  lockset_map_[curr_thd_id].Remove(addr);
}

void Eraser::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  EraserMeta *eraser_meta = dynamic_cast<EraserMeta *>(meta);
  DEBUG_ASSERT(eraser_meta);
  // get the current vector clock
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  EraserMeta::TLP &writer_lockset = eraser_meta->writer_lockset;
  EraserMeta::TLP &reader_lockset = eraser_meta->reader_lockset[curr_thd_id];

  // check writers
  if (writer_lockset.first != curr_thd_id && writer_lockset.second.IsDisjoint(curr_ls) && eraser_meta->writer_inst != NULL) {
    DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", eraser_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    eraser_meta->racy = true;
    // RAW race detected, report them
    thread_id_t thd_id = writer_lockset.first;
      DEBUG_ASSERT(eraser_meta->writer_inst != NULL);
      Inst *writer_inst = eraser_meta->writer_inst;
      // report the race
      ReportRace(eraser_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                 curr_thd_id, inst, RACE_EVENT_READ);
  }
  // update meta data
  reader_lockset.first = curr_thd_id;
  reader_lockset.second = curr_ls;
  eraser_meta->reader_inst_table[curr_thd_id] = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    eraser_meta->race_inst_set.insert(inst);
  }
}

void Eraser::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  EraserMeta *eraser_meta = dynamic_cast<EraserMeta *>(meta);
  DEBUG_ASSERT(eraser_meta);
  // get the current vector clock
  Lockset &curr_ls = lockset_map_[curr_thd_id];
  EraserMeta::TLP &writer_lockset = eraser_meta->writer_lockset;
  EraserMeta::TLPMap &reader_tlp_map = eraser_meta->reader_lockset;

  // check writers
  if (writer_lockset.first != curr_thd_id && writer_lockset.second.IsDisjoint(curr_ls) && eraser_meta->writer_inst != NULL) {
		DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
		DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", eraser_meta->addr);
		DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
		// mark the meta as racy
		eraser_meta->racy = true;
		// WAW race detected, report them
	    thread_id_t thd_id = writer_lockset.first;
		  DEBUG_ASSERT(eraser_meta->writer_inst != NULL);
		  Inst *writer_inst = eraser_meta->writer_inst;
		  // report the race
		  ReportRace(eraser_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
		             curr_thd_id, inst, RACE_EVENT_WRITE);
  }
  // check readers
  for (EraserMeta::TLPMap::iterator it = reader_tlp_map.begin(); it != reader_tlp_map.end(); ++it) {
	  EraserMeta::TLP &reader_lockset = it->second;
	if (reader_lockset.first != curr_thd_id && reader_lockset.second.IsDisjoint(curr_ls) && eraser_meta->reader_inst_table[reader_lockset.first] != NULL) {
	  DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
	  DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", eraser_meta->addr);
	  DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
	  // mark the meta as racy
	  eraser_meta->racy = true;
	  // WAR race detected, report them
	  thread_id_t thd_id = reader_lockset.first;
		  DEBUG_ASSERT(eraser_meta->reader_inst_table.find(thd_id) !=
		               eraser_meta->reader_inst_table.end());
		  Inst *reader_inst = eraser_meta->reader_inst_table[thd_id];
		  // report the race
		  ReportRace(eraser_meta, thd_id, reader_inst, RACE_EVENT_READ,
		             curr_thd_id, inst, RACE_EVENT_WRITE);
	}
  }
  // update meta data
  writer_lockset.first = curr_thd_id;
  writer_lockset.second = curr_ls;
  eraser_meta->writer_inst = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    eraser_meta->race_inst_set.insert(inst);
  }
}

void Eraser::ProcessFree(Meta *meta) {
  // cast the meta
  EraserMeta *eraser_meta = dynamic_cast<EraserMeta *>(meta);
  DEBUG_ASSERT(eraser_meta);
  // update racy inst set if needed
  if (track_racy_inst_ && eraser_meta->racy) {
    for (EraserMeta::InstSet::iterator it = eraser_meta->race_inst_set.begin();
         it != eraser_meta->race_inst_set.end(); ++it) {
      race_db_->SetRacyInst(*it, true);
    }
  }
  delete eraser_meta;
}

}
