#include "epoch.h"

namespace pintool {

bool Epoch::IsInitial() const {
	return epoch_.first == 0 && epoch_.second == 0;
}

bool Epoch::Equal(VectorClock *vc) const {
	return !IsInitial() && (epoch_.second == vc->GetClock(epoch_.first));
}

bool Epoch::HappensBefore(VectorClock *vc) const {
	return epoch_.second <= vc->GetClock(epoch_.first);
}

void Epoch::Make(thread_id_t tid, timestamp_t clock) {
	epoch_.first = tid;
	epoch_.second = clock;
}

bool operator==(const Epoch &e1, const Epoch &e2) { return e1.epoch_ == e2.epoch_; }
bool operator<(const Epoch &e1, const Epoch &e2) { return e1.epoch_ < e2.epoch_; }

}
