#ifndef PINTOOL_HISTLOCKPLUS_H_
#define PINTOOL_HISTLOCKPLUS_H_

#include <set>
#include <list>

#include "detector1.h"
#include "epoch.h"
#include "lockset.h"

namespace pintool {

    using namespace race;

    class HistLockPlus : public Detector {
    public:

        HistLockPlus() {
        }

        ~HistLockPlus() {
        }

    protected:
        // the meta data for the memory access

        class HistLockPlusMeta : public Meta {
        public:
            typedef std::pair<Epoch, int> ECtx;
            typedef std::pair<ECtx, Lockset> ELP;
            typedef std::pair<ELP, Inst*> ELPI;
            typedef std::list<ELPI> ELPIList;
            typedef std::list<ELPIList::iterator> ELPIIterList;
            typedef std::pair<address_t, ELPIIterList> CallSiteELPIList;
            typedef std::map<thread_id_t, CallSiteELPIList> CallSiteELPIListMap;

            explicit HistLockPlusMeta(address_t a) : Meta(a) {
            }

            ~HistLockPlusMeta() {
            }

            ELPIList writer_elpi_list;
            ELPIList reader_elpi_list;

            CallSiteELPIListMap writer_callsite_elpi_map;
            CallSiteELPIListMap reader_callsite_elpi_map;
        };

        struct iteratorcomp {

            bool operator()(const HistLockPlusMeta::ELPIList::iterator& lhs, const HistLockPlusMeta::ELPIList::iterator& rhs) const {
                return *lhs < *rhs;
            }
        };

        std::map<thread_id_t, Lockset> lockset_map_;
        std::map<thread_id_t, address_t> callsite_map_;
        std::map<thread_id_t, int> release_map_;

        // overrided virtual functions
        Meta *GetMeta(address_t iaddr);
        void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id);
        void AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                Inst *inst, thread_id_t child_thd_id);
        void BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                Inst *inst, address_t target);
        void ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
        void ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
        void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
        void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
        void ProcessFree(Meta *meta);

    private:
        DISALLOW_COPY_CONSTRUCTORS(HistLockPlus);
    };

}

#endif
