#ifndef PINTOOL_HISTLOCK_H_
#define PINTOOL_HISTLOCK_H_

#include <list>

#include "detector1.h"
#include "epoch.h"
#include "lockset.h"

namespace pintool {

using namespace race;

class HistLock : public Detector {
 public:
  HistLock() {}
  ~HistLock() {}

 protected:
  // the meta data for the memory access
  class HistLockMeta : public Meta {
   public:
	typedef std::pair<Epoch, Lockset> ELP;
	typedef std::pair<ELP, Inst*> ELPI;
	typedef std::list<ELPI> ELPIList;
	typedef std::list<ELPIList::iterator> ELPIIterList;
	typedef std::pair<address_t, ELPIIterList> CallSiteELPIList;
	typedef std::map<thread_id_t, CallSiteELPIList> CallSiteELPIListMap;

    explicit HistLockMeta(address_t a) : Meta(a) {}
    ~HistLockMeta() {}

    ELPIList writer_elpi_list;
    ELPIList reader_elpi_list;

    CallSiteELPIListMap writer_callsite_elpi_map;
    CallSiteELPIListMap reader_callsite_elpi_map;
  };

  std::map<thread_id_t, Lockset> lockset_map_;
  std::map<thread_id_t, address_t> callsite_map_;

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t target);
  void ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

 private:
  DISALLOW_COPY_CONSTRUCTORS(HistLock);
};

}

#endif
