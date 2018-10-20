#ifndef PINTOOL_LOCKSET_H_
#define PINTOOL_LOCKSET_H_

#include <set>
#include <sstream>
#include <algorithm>

#include "core/basictypes.h"

namespace pintool {

class Lockset {
 public:
  typedef std::set<address_t> AddrSet;

  Lockset() {}
  ~Lockset() {}

  void Add(address_t addr) { set_.insert(addr); }
  void Remove(address_t addr) { set_.erase(addr); }
  void Clear() { set_.clear(); }
  void Intersect(Lockset &ls);
  void Union(Lockset &ls);
  const AddrSet& GetAddrSet() const { return set_; }
  Lockset GetIntersection(Lockset &ls1, Lockset &ls2);
  Lockset GetUnion(Lockset &ls1, Lockset &ls2);

  bool IsEmpty() const { return set_.empty(); }
  bool IsExist(address_t addr) const { return set_.find(addr) != set_.end(); }
  bool IsMatch(const Lockset &ls) const;
  bool IsSubsetOf(const Lockset &ls) const;
  bool IsSupersetOf(const Lockset &ls) const;
  bool IsDisjoint(const Lockset &ls) const;

  std::string ToString();

  friend bool operator==(const Lockset &ls1, const Lockset &ls2);
  friend bool operator<(const Lockset &ls1, const Lockset &ls2);

 protected:
  AddrSet set_;
};

}

#endif
