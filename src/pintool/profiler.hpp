#ifndef PINTOOL_PROFILER_HPP_
#define PINTOOL_PROFILER_HPP_

#include "detector1.h"
#include "epoch.h"
#include "lockset.h"
#include <list>
#include <map>

#include "core/execution_control.hpp"

namespace pintool {

    class Profiler : public ExecutionControl {
    public:

        Profiler() : race_db_(NULL), detector_(NULL) {
        }

        ~Profiler() {
            delete race_db_;
            delete detector_;
        }

    protected:
        void HandlePreSetup();
        void HandlePostSetup();
        bool HandleIgnoreMemAccess(IMG img);
        void HandleProgramExit();

        RaceDB *race_db_;
        Detector1 *detector_;

    private:
        DISALLOW_COPY_CONSTRUCTORS(Profiler);
    };

}

#endif
