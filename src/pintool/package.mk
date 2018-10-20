srcs += \
  pintool/epoch.cpp \
  pintool/lockset.cpp \
  pintool/detector1.cpp \
  pintool/profiler.cpp \
  pintool/profiler_main.cpp \
  pintool/histlockplus.cpp \

pintools += histlockplus.so

histlockplus_objs := \
  pintool/epoch.o \
  pintool/lockset.o \
  pintool/detector1.o \
  pintool/profiler.o \
  pintool/profiler_main.o \
  race/race.o \
  race/race.pb.o \
  pintool/histlockplus.o \
  $(core_objs)
