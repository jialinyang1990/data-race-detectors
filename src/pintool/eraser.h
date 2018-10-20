#ifndef PINTOOL_ERASER_H_
#define PINTOOL_ERASER_H_

#include "detector1.h"
#include "lockset.h"

namespace pintool {

using namespace race;

class Eraser : public Detector1 {
 public:
  Eraser() {}
  ~Eraser() {}

 protected:
  // the meta data for the memory access
  class EraserMeta : public Meta {
   public:
	typedef std::pair<thread_id_t, Lockset> TLP;
	typedef std::map<thread_id_t, TLP> TLPMap;
	typedef std::map<thread_id_t, Inst *> InstMap;

    explicit EraserMeta(address_t a) : Meta(a), writer_inst(NULL) {}
    ~EraserMeta() {}

    TLP writer_lockset;
    Inst* writer_inst;
    TLPMap reader_lockset;
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
  DISALLOW_COPY_CONSTRUCTORS(Eraser);
};

}

#endif
