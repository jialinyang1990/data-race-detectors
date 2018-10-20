#ifndef PINTOOL_EPOCH_H_
#define PINTOOL_EPOCH_H_

#include "core/basictypes.h"
#include "core/vector_clock.h"

namespace pintool {

class Epoch {
 public:
  Epoch() : epoch_(0, 0) {}
  ~Epoch() {}

  bool IsInitial() const;
  bool Equal(VectorClock *vc) const;
  bool HappensBefore(VectorClock *vc) const;
  void Make(thread_id_t tid, timestamp_t clock);

  inline std::pair<thread_id_t, timestamp_t> GetEpoch() const { return epoch_; }
  inline thread_id_t GetTid() const { return epoch_.first; }
  inline timestamp_t GetClock() const { return epoch_.second; }

  friend bool operator==(const Epoch &e1, const Epoch &e2);
  friend bool operator<(const Epoch &e1, const Epoch &e2);

 private:
  std::pair<thread_id_t, timestamp_t> epoch_;

};

}

#endif
