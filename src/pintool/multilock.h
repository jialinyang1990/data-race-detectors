#ifndef PINTOOL_MULTILOCK_H_
#define PINTOOL_MULTILOCK_H_

#include <climits>
#include <tr1/unordered_set>
#include <tr1/unordered_map>

#include "detector1.h"
#include "epoch.h"
#include "lockset.h"

namespace pintool {

using namespace race;

class MultiLock : public Detector {
 public:
  MultiLock() {}
  ~MultiLock() {}

 protected:
  // the meta data for the memory access
  class MultiLockMeta : public Meta {
   public:

	class ELPHash;

	typedef std::pair<Epoch, Lockset> ELP;
	typedef std::tr1::unordered_set<ELP, ELPHash> ELPSet;
	typedef std::map<thread_id_t, ELPSet> ELSMap;
	typedef std::map<ELP, Inst*> InstMap;

    explicit MultiLockMeta(address_t a) : Meta(a) {}
    ~MultiLockMeta() {}

    ELSMap writer_els_map;
    ELSMap reader_els_map;
    InstMap writer_inst_table;
    InstMap reader_inst_table;

    class ELPHash {
    public:
    	size_t operator() (const ELP &elp) const {
    	    		const Epoch &e = elp.first;
    	    		ushort shift = (sizeof(size_t) * CHAR_BIT) / 4;
    	    		size_t e_hash = (size_t) (e.GetTid() << shift * 3) | e.GetClock();

    	    		size_t ls_hash = 0;
    	    		const Lockset &ls = elp.second;
    	    		const Lockset::AddrSet &s = ls.GetAddrSet();
    	    		for (Lockset::AddrSet::iterator it = s.begin(); it != s.end(); ++it) {
    	    			ls_hash = (ls_hash << 1) ^ (size_t) *it;
    	    		}
    	    		return e_hash ^ ls_hash;
    	}
    };

  };

  std::map<thread_id_t, Lockset> lockset_map_;

  bool UpdateOnRead(thread_id_t curr_thd_id, MultiLockMeta *multilock_meta,
		            VectorClock *curr_vc, Lockset &curr_ls, Inst *inst);
  bool UpdateOnWrite(thread_id_t curr_thd_id, MultiLockMeta *multilock_meta,
                     VectorClock *curr_vc, Lockset &curr_ls, Inst *inst);

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
		                Inst *inst, thread_id_t child_thd_id);
  void ProcessLock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessUnlock(thread_id_t curr_thd_id, address_t addr, MutexMeta *meta);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

 private:
  DISALLOW_COPY_CONSTRUCTORS(MultiLock);
};

}

#endif
