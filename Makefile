PROGRAM_     = nbody

MCC         ?= fpgacc
MCC_         = $(CROSS_COMPILE)$(MCC)
GCC_         = $(CROSS_COMPILE)gcc
CFLAGS_      = $(CFLAGS) -O3 -std=gnu99 -funsafe-math-optimizations
MCC_FLAGS_   = --ompss
MCC_FLAGS_I_ = --instrument
MCC_FLAGS_D_ = --debug -g -k
LDFLAGS_     = $(LDFLAGS) -lm

# FPGA bitstream Variables
FPGA_HWRUNTIME         ?= som
FPGA_CLOCK             ?= 200
FPGA_MEMORY_PORT_WIDTH ?= 128
NBODY_BLOCK_SIZE       ?= 2048
NBODY_NCALCFORCES      ?= 8
NBODY_NUM_FBLOCK_ACCS  ?= 1

CFLAGS_ += -DNBODY_BLOCK_SIZE=$(NBODY_BLOCK_SIZE) -DNBODY_NCALCFORCES=$(NBODY_NCALCFORCES) -DNBODY_NUM_FBLOCK_ACCS=$(NBODY_NUM_FBLOCK_ACCS) -DFPGA_HWRUNTIME=\"$(FPGA_HWRUNTIME)\" -DFPGA_MEMORY_PORT_WIDTH=$(FPGA_MEMORY_PORT_WIDTH)
FPGA_LINKER_FLAGS_ =--Wf,--name=$(PROGRAM_),--board=$(BOARD),-c=$(FPGA_CLOCK),--hwruntime=$(FPGA_HWRUNTIME),--interconnect_opt=performance

ifdef INTERCONNECT_REGSLICE
	FPGA_LINKER_FLAGS_ += --Wf,--interconnect_regslice,$(INTERCONNECT_REGSLICE)
endif

ifeq ($(FPGA_HWRUNTIME),pom)
	FPGA_LINKER_FLAGS_ += --Wf,--picos_max_deps_per_task=3,--picos_max_args_per_task=11,--picos_max_copies_per_task=11,--picos_tm_size=32,--picos_dm_size=102,--picos_vm_size=102
endif

all: help
help:
	@echo 'Supported targets:       $(PROGRAM_)-p $(PROGRAM_)-i $(PROGRAM_)-d $(PROGRAM_)-seq bitstream-i bitstream-p clean info help'
	@echo 'Environment variables:   CFLAGS, LDFLAGS, CROSS_COMPILE, MCC'
	@echo 'FPGA env. variables:     BOARD, FPGA_HWRUNTIME, FPGA_CLOCK, FPGA_MEMORY_PORT_WIDTH, NBODY_BLOCK_SIZE, NBODY_NCALCFORCES, NBODY_NUM_FBLOCK_ACCS'

$(PROGRAM_)-p: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) -DRUNTIME_MODE=\"perf\" $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-i:  ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $(MCC_FLAGS_I_) -DRUNTIME_MODE=\"instr\" $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-d:  ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $(MCC_FLAGS_D_) -DRUNTIME_MODE=\"debug\" $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-seq: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(GCC_) $(CFLAGS_) -DRUNTIME_MODE=\"seq\" $^ -o $@ $(LDFLAGS_)

bitstream-i: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	sed -i 's/num_instances(.)/num_instances($(NBODY_NUM_FBLOCK_ACCS))/1' ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $(MCC_FLAGS_I_) -DRUNTIME_MODE=\"instr\" $^ -o $(PROGRAM_)-i $(LDFLAGS_) \
	--bitstream-generation $(FPGA_LINKER_FLAGS_) \
	--variable=fpga_memory_port_width:$(FPGA_MEMORY_PORT_WIDTH)

bitstream-p: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	sed -i 's/num_instances(.)/num_instances($(NBODY_NUM_FBLOCK_ACCS))/1' ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) -DRUNTIME_MODE=\"perf\" $^ -o $(PROGRAM_)-p $(LDFLAGS_) \
	--bitstream-generation $(FPGA_LINKER_FLAGS_) \
	--variable=fpga_memory_port_width:$(FPGA_MEMORY_PORT_WIDTH)

clean:
	rm -fv *.o $(PROGRAM_)-? $(MCC_)_$(PROGRAM_).c *:*_hls_automatic_mcxx.cpp
