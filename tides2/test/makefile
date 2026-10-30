PACKAGES       = tides2/test stmlib/utils tides2 stmlib/dsp tides2/ramp

VPATH          = $(PACKAGES)

TARGET         = tides2_test
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
CC_FILES       = tides_test.cc \
		poly_slope_generator.cc \
		ramp_extractor.cc \
		resources.cc \
		units.cc
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk

all:  tides2_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -g -Wall -Werror -msse2 -Wno-unused-variable -O2 -I. $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -I. $< -MF $@ -MT $(@:.d=.o)

tides2_test:  $(OBJS)
	g++ -g -o $(TARGET) $(OBJS) -Wl,-no_pie -lm -lprofiler -L/opt/local/lib

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

clean:
	rm $(BUILD_DIR)*.*

include $(DEP_FILE)
