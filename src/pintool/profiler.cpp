
#include "profiler.hpp"

namespace pintool {

    void Profiler::HandlePreSetup() {
        ExecutionControl::HandlePreSetup();

        knob_->RegisterBool("ignore_lib", "whether ignore accesses from common libraries", "1");
        knob_->RegisterStr("race_in", "the input race database path", "race.db");
        knob_->RegisterStr("race_out", "the output race database path", "race.db");

        detector_ = new Case0();
        detector_->Register();
    }

    void Profiler::HandlePostSetup() {
        ExecutionControl::HandlePostSetup();

        // load race db
        race_db_ = new RaceDB(CreateMutex());
        race_db_->Load(knob_->ValueStr("race_in"), sinfo_);

        // add data race detector
        if (detector_->Enabled()) {
            detector_->Setup(CreateMutex(), race_db_);
            AddAnalyzer(detector_);
        }

        // make sure that we use one data race detector
        if (!detector_)
            Abort("please choose a data race detector\n");
    }

    bool Profiler::HandleIgnoreMemAccess(IMG img) {
        if (!IMG_Valid(img))
            return true;
        Image *image = sinfo_->FindImage(IMG_Name(img));
        DEBUG_ASSERT(image);
        if (image->IsPthread())
            return true;
        if (knob_->ValueBool("ignore_lib")) {
            if (image->IsCommonLib())
                return true;
        }
        return false;
    }

    void Profiler::HandleProgramExit() {
        ExecutionControl::HandleProgramExit();

        // save race db
        race_db_->Save(knob_->ValueStr("race_out"), sinfo_);

        std::cout << "------------------------------------------------------------" << std::endl;
        std::cout << "start, join: " << detector_->thread_start_c_ << ", " << detector_->thread_join_c_ << std::endl;
        std::cout << "instructions read, write, atom: " << detector_->read_c_ << ", " << detector_->write_c_ << ", " << detector_->atom_c_ << std::endl;
        std::cout << "locations read, write: " << detector_->loc_r_c_ << ", " << detector_->loc_w_c_ << std::endl;
        std::cout << "lock, unlock: " << detector_->lock_c_ << ", " << detector_->unlock_c_ << std::endl;
        std::cout << "signal, broadcast: " << detector_->signal_c_ << ", " << detector_->broadcast_c_ << std::endl;
        std::cout << "wait_before, wait_after: " << detector_->wait_b_c_ << ", " << detector_->wait_a_c_ << std::endl;
        std::cout << "time_wait_before, time_wait_after: " << detector_->timew_b_c_ << ", " << detector_->timew_a_c_ << std::endl;
        std::cout << "barrier_before, barrier_after: " << detector_->barrier_b_c_ << ", " << detector_->barrier_a_c_ << std::endl;
        std::cout << "skip write, skip read: " << detector_->skip_w_c_ << ", " << detector_->skip_r_c_ << std::endl;
        std::cout << "eliminate write, eliminate read: " << detector_->eli_w_c_ << ", " << detector_->eli_r_c_ << std::endl;
        std::cout << "time eliminate write, read: " << detector_->time_eli_write << ", " << detector_->time_eli_read << std::endl;
        std::cout << "detect wr, ww, rw: " << detector_->detect_wr_ << ", " << detector_->detect_ww_ << ", " << detector_->detect_rw_ << std::endl;
        std::cout << "update r, w: " << detector_->update_r_ << ", " << detector_->update_w_ << std::endl;
        std::cout << "------------------------------------------------------------" << std::endl;

    }

}
