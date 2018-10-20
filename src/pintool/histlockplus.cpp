//#define WINDOW_SIZE 100

#include "histlockplus.h"

namespace pintool {

    using namespace race;

    HistLockPlus::Meta *HistLockPlus::GetMeta(address_t iaddr) {
        Meta::Table::iterator it = meta_table_.find(iaddr);
        if (it == meta_table_.end()) {
            Meta *meta = new HistLockPlusMeta(iaddr);
            meta_table_[iaddr] = meta;
            return meta;
        } else {
            return it->second;
        }
    }

    void HistLockPlus::ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id) {
        Detector::ThreadStart(curr_thd_id, parent_thd_id);
        releaseCnt_map_[curr_thd_id] = 0;
    }

    void HistLockPlus::AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
            Inst *inst, thread_id_t child_thd_id) {
        Detector::AfterPthreadJoin(curr_thd_id, curr_thd_clk, inst, child_thd_id);
        VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
        DEBUG_ASSERT(curr_vc);
        // increment the vector clock
        curr_vc->Increment(curr_thd_id);
    }

    void HistLockPlus::BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
            Inst *inst, address_t target) {
        callsite_map_[curr_thd_id] = target;
    }

    void HistLockPlus::ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
        lockset_map_[curr_thd_id].Add(addr);
    }

    void HistLockPlus::ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta) {
        lockset_map_[curr_thd_id].Remove(addr);
        releaseCnt_map_[curr_thd_id]++;
    }

    void HistLockPlus::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
        // cast the meta
        HistLockPlusMeta *histlockplus_meta = dynamic_cast<HistLockPlusMeta *> (meta);
        DEBUG_ASSERT(histlockplus_meta);
        // get the current vector clock
        VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
        Lockset &curr_ls = lockset_map_[curr_thd_id];
        HistLockPlusMeta::ELPIList &reader_elpi_list = histlockplus_meta->reader_elpi_list;
        HistLockPlusMeta::ELPIList &writer_elpi_list = histlockplus_meta->writer_elpi_list;
        HistLockPlusMeta::CallSiteELPIList &reader_callsite = histlockplus_meta->reader_callsite_elpi_map[curr_thd_id];

        if (!reader_elpi_list.empty()) {
            HistLockPlusMeta::ECtx &last_r = histlockplus_meta->reader_elpi_list.back().first.first;
            if (last_r.first.Equal(curr_vc) && last_r.second == releaseCnt_map_[curr_thd_id]) {
                skip_r_c_++;
                return;
            }
        }
        if (!writer_elpi_list.empty()) {
            HistLockPlusMeta::ECtx &last_w = histlockplus_meta->writer_elpi_list.back().first.first;
            if (last_w.first.Equal(curr_vc) && last_w.second == releaseCnt_map_[curr_thd_id]) {
                skip_w_c_++;
                return;
            }
        }

        // check writers
        for (HistLockPlusMeta::ELPIList::iterator it = writer_elpi_list.begin(); it != writer_elpi_list.end(); ++it) {
            detect_wr_++;
            Epoch &writer_epoch = it->first.first.first;
            Lockset &writer_lockset = it->first.second;
            if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
                DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
                DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlockplus_meta->addr);
                DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
                // mark the meta as racy
                histlockplus_meta->racy = true;
                // RAW race detected, report them
                thread_id_t thd_id = writer_epoch.GetTid();
                timestamp_t clk = writer_epoch.GetClock();
                if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
                    DEBUG_ASSERT(it->second != NULL);
                    Inst *writer_inst = it->second;
                    // report the race
                    ReportRace(histlockplus_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                            curr_thd_id, inst, RACE_EVENT_READ);
                }
            }
        }

        clock_t t = clock();
        if (reader_callsite.first != callsite_map_[curr_thd_id]) {
            std::set<HistLockPlusMeta::ELPIList::iterator, iteratorcomp> myset;
            for (HistLockPlusMeta::ELPIIterList::iterator it = reader_callsite.second.begin(); it != reader_callsite.second.end(); ++it) {
                HistLockPlusMeta::ELPIIterList::iterator it2 = it;
                while (++it2 != reader_callsite.second.end()) {
                    HistLockPlusMeta::ELPIList::iterator myit = *it;
                    const Epoch &epoch = myit->first.first.first;
                    const Lockset &lockset = myit->first.second;
                    HistLockPlusMeta::ELPIList::iterator myit2 = *it2;
                    const Epoch &epoch2 = myit2->first.first.first;
                    const Lockset &lockset2 = myit2->first.second;
                    if (epoch == epoch2 && lockset.IsSubsetOf(lockset2)) {
                        myset.insert(myit);
                    } else if (epoch == epoch2 && lockset2.IsSubsetOf(lockset)) {
                        myset.insert(myit2);
                    }
                }
            }
            for (std::set<HistLockPlusMeta::ELPIList::iterator>::iterator ii = myset.begin(); ii != myset.end(); ii++) {
                eli_r_c_++;
                reader_elpi_list.erase(*ii);
            }
            reader_callsite.first = callsite_map_[curr_thd_id];
            reader_callsite.second.clear();
            myset.clear();
        }
        t = clock() - t;
        time_eli_read += ((float) t) / CLOCKS_PER_SEC;

        // update meta data
        update_r_++;
        Epoch curr_epoch;
        curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
        HistLockPlusMeta::ECtx curr_ectx(curr_epoch, releaseCnt_map_[curr_thd_id]);
        HistLockPlusMeta::ELP elp(curr_ectx, curr_ls);
        HistLockPlusMeta::ELPI elpi(elp, inst);
        HistLockPlusMeta::ELPIList::iterator position = reader_elpi_list.insert(reader_elpi_list.end(), elpi);
        histlockplus_meta->reader_callsite_elpi_map[curr_thd_id].second.push_back(position);

        //  if (reader_elpi_list.size() > WINDOW_SIZE) {
        //	  reader_elpi_list.pop_front();
        //  }

        // update race inst set if needed
        if (track_racy_inst_) {
            histlockplus_meta->race_inst_set.insert(inst);
        }
    }

    void HistLockPlus::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
        // cast the meta
        HistLockPlusMeta *histlockplus_meta = dynamic_cast<HistLockPlusMeta *> (meta);
        DEBUG_ASSERT(histlockplus_meta);
        // get the current vector clock
        VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
        Lockset &curr_ls = lockset_map_[curr_thd_id];
        HistLockPlusMeta::ELPIList &reader_elpi_list = histlockplus_meta->reader_elpi_list;
        HistLockPlusMeta::ELPIList &writer_elpi_list = histlockplus_meta->writer_elpi_list;
        HistLockPlusMeta::CallSiteELPIList &writer_callsite = histlockplus_meta->writer_callsite_elpi_map[curr_thd_id];

        if (!writer_elpi_list.empty()) {
            HistLockPlusMeta::ECtx &last_w = histlockplus_meta->writer_elpi_list.back().first.first;
            if (last_w.first.Equal(curr_vc) && last_w.second == releaseCnt_map_[curr_thd_id]) {
                skip_w_c_++;
                return;
            }
        }

        // check writers
        for (HistLockPlusMeta::ELPIList::iterator it = writer_elpi_list.begin(); it != writer_elpi_list.end(); ++it) {
            detect_ww_++;
            Epoch &writer_epoch = it->first.first.first;
            Lockset &writer_lockset = it->first.second;
            if (!writer_epoch.HappensBefore(curr_vc) && writer_lockset.IsDisjoint(curr_ls)) {
                DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
                DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlockplus_meta->addr);
                DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
                // mark the meta as racy
                histlockplus_meta->racy = true;
                // WAW race detected, report them
                thread_id_t thd_id = writer_epoch.GetTid();
                timestamp_t clk = writer_epoch.GetClock();
                if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
                    DEBUG_ASSERT(it->second != NULL);
                    Inst *writer_inst = it->second;
                    // report the race
                    ReportRace(histlockplus_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                            curr_thd_id, inst, RACE_EVENT_WRITE);
                }
            }
        }
        // check readers
        for (HistLockPlusMeta::ELPIList::iterator it = reader_elpi_list.begin(); it != reader_elpi_list.end(); ++it) {
            detect_rw_++;
            Epoch &reader_epoch = it->first.first.first;
            Lockset &reader_lockset = it->first.second;
            if (!reader_epoch.HappensBefore(curr_vc) && reader_lockset.IsDisjoint(curr_ls)) {
                DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
                DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", histlockplus_meta->addr);
                DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
                // mark the meta as racy
                histlockplus_meta->racy = true;
                // WAR race detected, report them
                thread_id_t thd_id = reader_epoch.GetTid();
                timestamp_t clk = reader_epoch.GetClock();
                if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
                    DEBUG_ASSERT(it->second != NULL);
                    Inst *reader_inst = it->second;
                    // report the race
                    ReportRace(histlockplus_meta, thd_id, reader_inst, RACE_EVENT_READ,
                            curr_thd_id, inst, RACE_EVENT_WRITE);
                }
            }
        }

        clock_t t = clock();
        if (writer_callsite.first != callsite_map_[curr_thd_id]) {
            std::set<HistLockPlusMeta::ELPIList::iterator, iteratorcomp> myset;
            for (HistLockPlusMeta::ELPIIterList::iterator it = writer_callsite.second.begin(); it != writer_callsite.second.end(); ++it) {
                HistLockPlusMeta::ELPIIterList::iterator it2 = it;
                while (++it2 != writer_callsite.second.end()) {
                    HistLockPlusMeta::ELPIList::iterator myit = *it;
                    const Epoch &epoch = myit->first.first.first;
                    const Lockset &lockset = myit->first.second;
                    HistLockPlusMeta::ELPIList::iterator myit2 = *it2;
                    const Epoch &epoch2 = myit2->first.first.first;
                    const Lockset &lockset2 = myit2->first.second;
                    if (epoch == epoch2 && lockset.IsSubsetOf(lockset2)) {
                        myset.insert(myit);
                    } else if (epoch == epoch2 && lockset2.IsSubsetOf(lockset)) {
                        myset.insert(myit2);
                    }
                }
            }
            for (std::set<HistLockPlusMeta::ELPIList::iterator>::iterator ii = myset.begin(); ii != myset.end(); ii++) {
                eli_w_c_++;
                writer_elpi_list.erase(*ii);
            }
            writer_callsite.first = callsite_map_[curr_thd_id];
            writer_callsite.second.clear();
            myset.clear();
        }
        t = clock() - t;
        time_eli_write += ((float) t) / CLOCKS_PER_SEC;

        // update meta data
        update_w_++;
        Epoch curr_epoch;
        curr_epoch.Make(curr_thd_id, curr_vc->GetClock(curr_thd_id));
        HistLockPlusMeta::ECtx curr_ectx(curr_epoch, releaseCnt_map_[curr_thd_id]);
        HistLockPlusMeta::ELP elp(curr_ectx, curr_ls);
        HistLockPlusMeta::ELPI elpi(elp, inst);
        HistLockPlusMeta::ELPIList::iterator position = writer_elpi_list.insert(writer_elpi_list.end(), elpi);
        histlockplus_meta->writer_callsite_elpi_map[curr_thd_id].second.push_back(position);

        //  if (writer_elpi_list.size() > WINDOW_SIZE) {
        //	  writer_elpi_list.pop_front();
        //  }

        // update race inst set if needed
        if (track_racy_inst_) {
            histlockplus_meta->race_inst_set.insert(inst);
        }
    }

    void HistLockPlus::ProcessFree(Meta *meta) {
        // cast the meta
        HistLockPlusMeta *histlockplus_meta = dynamic_cast<HistLockPlusMeta *> (meta);
        DEBUG_ASSERT(histlockplus_meta);
        // update racy inst set if needed
        if (track_racy_inst_ && histlockplus_meta->racy) {
            for (HistLockPlusMeta::InstSet::iterator it = histlockplus_meta->race_inst_set.begin();
                    it != histlockplus_meta->race_inst_set.end(); ++it) {
                race_db_->SetRacyInst(*it, true);
            }
        }
        delete histlockplus_meta;
    }

}
