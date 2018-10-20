#ifndef PINTOOL_DETECTOR1_H_
#define PINTOOL_DETECTOR1_H_

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/vector_clock.h"
#include "core/filter.h"
#include "core/logging.h"
#include "race/race.h"
#include <queue>

namespace pintool {

using namespace race;

class Detector1 : public Analyzer {
 public:
  Detector1();
  virtual ~Detector1();

  virtual void Register();
  virtual bool Enabled() { return true; }
  virtual void Setup(Mutex *lock, RaceDB *race_db);
  virtual void ImageLoad(Image *image,
                         address_t low_addr, address_t high_addr,
                         address_t data_start, size_t data_size,
                         address_t bss_start, size_t bss_size);
  virtual void ImageUnload(Image *image,
                           address_t low_addr, address_t high_addr,
                           address_t data_start, size_t data_size,
                           address_t bss_start, size_t bss_size);
  virtual void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id);
  virtual void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size);
  virtual void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size);
  virtual void BeforeAtomicInst(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                std::string type, address_t addr);
  virtual void AfterAtomicInst(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               std::string type, address_t addr);
  virtual void AfterPthreadJoin(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                thread_id_t child_thd_id);
  virtual void AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk,
                                     Inst *inst, address_t addr);
  virtual void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr);
  virtual void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr);
  virtual void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk,
                                          Inst *inst, address_t addr);
  virtual void BeforePthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr, address_t mutex_addr);
  virtual void AfterPthreadCondWait(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk, Inst *inst,
                                    address_t cond_addr, address_t mutex_addr);
  virtual void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr);
  virtual void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t cond_addr,
                                         address_t mutex_addr);
  virtual void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr);
  virtual void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr);
  virtual void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr);
  virtual void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t nmemb, size_t size,
                           address_t addr);
  virtual void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size);
  virtual void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t ori_addr, size_t size,
                            address_t new_addr);
  virtual void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t addr);
  virtual void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr);

  // counters
  int thread_start_c_, thread_join_c_;
  int read_c_, write_c_, atom_c_;
  int loc_r_c_, loc_w_c_;
  int lock_c_, unlock_c_;
  int signal_c_, broadcast_c_;
  int wait_b_c_, wait_a_c_;
  int timew_b_c_, timew_a_c_;
  int barrier_b_c_, barrier_a_c_;
  int skip_w_c_, skip_r_c_;
  int eli_w_c_, eli_r_c_;
  float time_eli_write, time_eli_read;
  int detect_ww_, detect_rw_, detect_wr_, update_w_, update_r_;

 protected:
  // the abstract meta data for the memory access
  class Meta {
   public:
    typedef std::tr1::unordered_map<address_t, Meta *> Table;
    typedef std::set<Inst *> InstSet;

    explicit Meta(address_t a) : addr(a), racy(false) {}
    virtual ~Meta() {}

    address_t addr;
    InstSet race_inst_set;
    bool racy; // whether this meta is involved in any race
  };

  // the meta data for mutex variables to track vector clock
  class MutexMeta {
   public:
    typedef std::tr1::unordered_map<address_t, MutexMeta *> Table;

    MutexMeta() {}
    ~MutexMeta() {}

    VectorClock vc;
  };

  // the meta data for conditional variables to track vector clock
  class CondMeta {
   public:
    typedef std::map<thread_id_t, VectorClock> VectorClockMap;
    typedef std::tr1::unordered_map<address_t, CondMeta *> Table;

    CondMeta() {}
    ~CondMeta() {}

    VectorClockMap wait_table;
    VectorClockMap signal_table;
  };

  // the meta data for barrier variables to track vector clock
  class BarrierMeta {
   public:
    typedef std::map<thread_id_t, std::pair<VectorClock, bool> > VectorClockMap;
    typedef std::tr1::unordered_map<address_t, BarrierMeta *> Table;

    BarrierMeta()
        : pre_using_table1(true),
          post_using_table1(true) {}

    ~BarrierMeta() {}

    bool pre_using_table1;
    bool post_using_table1;
    VectorClockMap barrier_wait_table1;
    VectorClockMap barrier_wait_table2;
  };

  // helper functions
  void InitCounters();
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr) { return filter_->Filter(addr, false); }
  MutexMeta *GetMutexMeta(address_t iaddr);
  CondMeta *GetCondMeta(address_t iaddr);
  BarrierMeta *GetBarrierMeta(address_t iaddr);
  void ReportRace(Meta *meta, thread_id_t t0, Inst *i0, RaceEventType p0,
                  thread_id_t t1, Inst *i1, RaceEventType p1);

  // main processing functions
  virtual void ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  virtual void ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessNotify(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreWait(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPostWait(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);
  void ProcessPostBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);
  void ProcessFree(MutexMeta *meta);
  void ProcessFree(CondMeta *meta);
  void ProcessFree(BarrierMeta *meta);

  // virtual functions to override
  virtual Meta *GetMeta(address_t iaddr) { return NULL; }
  virtual void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {}
  virtual void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {}
  virtual void ProcessFree(Meta *meta) {}

  // common databases
  Mutex *internal_lock_;
  RaceDB *race_db_;

  // settings and flasg
  address_t unit_size_;
  RegionFilter *filter_;
  bool track_racy_inst_;

  // meta data
  MutexMeta::Table mutex_meta_table_;
  CondMeta::Table cond_meta_table_;
  BarrierMeta::Table barrier_meta_table_;
  Meta::Table meta_table_;

  // global analysis state
  std::map<thread_id_t, VectorClock *> curr_vc_map_;
  std::map<thread_id_t, bool> atomic_map_; // whether executing atomic inst.

 private:
  DISALLOW_COPY_CONSTRUCTORS(Detector1);
};

}

#endif
