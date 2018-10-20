#ifndef PINTOOL_DJIT_H_
#define PINTOOL_DJIT_H_

#include "detector1.h"

namespace pintool {

using namespace race;

class Djit : public Detector {
 public:
  Djit() {}
  ~Djit() {}

 protected:
  // the meta data for the memory access
  class DjitMeta : public Meta {
   public:
	typedef std::map<thread_id_t, Inst *> InstMap;

    explicit DjitMeta(address_t a) : Meta(a) {}
    ~DjitMeta() {}

    VectorClock writer_vc;
    VectorClock reader_vc;
    InstMap writer_inst_table;
    InstMap reader_inst_table;
  };

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

 private:
  DISALLOW_COPY_CONSTRUCTORS(Djit);
};

}

#endif
