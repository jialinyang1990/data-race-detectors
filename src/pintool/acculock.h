#ifndef PINTOOL_ACCULOCK_H_
#define PINTOOL_ACCULOCK_H_

#include "detector1.h"
#include "epoch.h"
#include "lockset.h"

namespace pintool {

using namespace race;

class AccuLock : public Detector {
 public:
  AccuLock() {}
  ~AccuLock() {}

 protected:
  // the meta data for the memory access
  class AccuLockMeta : public Meta {
   public:
	typedef std::pair<Epoch, Lockset> ELP;
	typedef std::map<thread_id_t, ELP> ELPMap;
    typedef std::map<thread_id_t, Inst *> InstMap;

    explicit AccuLockMeta(address_t a) : Meta(a), writer_inst(NULL) {}
    ~AccuLockMeta() {}

    ELP writer_elp;
    ELPMap reader_elp_map;
    Inst* writer_inst;
    InstMap reader_inst_table;
  };

  std::map<thread_id_t, Lockset> lockset_map_;

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

 private:
  DISALLOW_COPY_CONSTRUCTORS(AccuLock);
};

}

#endif
