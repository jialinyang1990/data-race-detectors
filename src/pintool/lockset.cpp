#include "lockset.h"

namespace pintool {

void Lockset::Intersect(Lockset &ls) {
	for (AddrSet::iterator it = set_.begin(); it != set_.end(); ++it) {
			if(!ls.IsExist((address_t)*it))
				set_.erase(it);
	}
}

void Lockset::Union(Lockset &ls) {
	const AddrSet &s = ls.GetAddrSet();
	for (AddrSet::iterator it = s.begin(); it != s.end(); ++it) {
				set_.insert((address_t)*it);
	}
}

bool Lockset::IsSubsetOf(const Lockset &ls) const {
	if (set_.size() > ls.GetAddrSet().size())
	    return false;
	for (AddrSet::iterator it = set_.begin(); it != set_.end(); ++it) {
			if(!ls.IsExist((address_t)*it))
				return false;
	}
	return true;
}

bool Lockset::IsSupersetOf(const Lockset &ls) const {
	if (set_.size() < ls.GetAddrSet().size())
	    return false;
	const AddrSet &s = ls.GetAddrSet();
	for (AddrSet::iterator it = s.begin(); it != s.end(); ++it) {
			if(!IsExist((address_t)*it))
				return false;
	}
	return true;
}

bool Lockset::IsMatch(const Lockset &ls) const {
  if (set_.size() != ls.GetAddrSet().size())
    return false;
  for (AddrSet::iterator it = set_.begin(); it != set_.end(); ++it) {
		if(!ls.IsExist((address_t)*it))
			return false;
  }
  return true;
}

bool Lockset::IsDisjoint(const Lockset &ls) const {
  for (AddrSet::iterator it = set_.begin(); it != set_.end(); ++it) {
	if(ls.IsExist((address_t)*it))
		return false;
  }
  return true;
}

std::string Lockset::ToString() {
  std::stringstream ss;
  ss << "[";
  for (AddrSet::iterator it = set_.begin(); it != set_.end(); ++it) {
    ss << std::hex << "0x" << (address_t)*it << " ";
  }
  ss << "]";
  return ss.str();
}

bool operator==(const Lockset &ls1, const Lockset &ls2) {
	return ls1.IsMatch(ls2);
}

bool operator<(const Lockset &ls1, const Lockset &ls2) {
	if (ls1.GetAddrSet().size() < ls2.GetAddrSet().size())
		return true;
	else if (ls1.GetAddrSet().size() > ls2.GetAddrSet().size())
		return false;
	else {
		address_t a1 = 0, a2 = 0;
		const Lockset::AddrSet &s1 = ls1.GetAddrSet();
		for (Lockset::AddrSet::iterator it = s1.begin(); it != s1.end(); ++it) {
			a1 = (a1 << 1) ^ *it;
		}
		const Lockset::AddrSet &s2 = ls2.GetAddrSet();
		for (Lockset::AddrSet::iterator it = s2.begin(); it != s2.end(); ++it) {
			a2 = (a2 << 1) ^ *it;
		}
		return a1 < a2;
	}
}

}
