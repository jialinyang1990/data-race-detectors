#ifndef PINTOOL_FASTTRACK_H_
#define PINTOOL_FASTTRACK_H_

#include "detector1.h"
#include "epoch.h"

namespace pintool {

using namespace race;

class FastTrack : public Detector1 {
 public:
  FastTrack() {}
  ~FastTrack() {}

 protected:
  // the meta data for the memory access
  class FastTrackMeta : public Meta {
   public:
	typedef std::map<thread_id_t, Inst *> InstMap;

    explicit FastTrackMeta(address_t a) : Meta(a), read_shared(false), writer_inst(NULL) {}
    ~FastTrackMeta() {}

    bool read_shared;
    Epoch writer_epoch;
    Epoch reader_epoch;
    VectorClock reader_vc;
    Inst* writer_inst;
    InstMap reader_inst_table;
  };

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

 private:
  DISALLOW_COPY_CONSTRUCTORS(FastTrack);
};

}

#endif
