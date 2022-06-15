.PHONY: clean info
all: help

PROGRAM_     = nbody

MCC         ?= fpgacc
MCC_         = $(CROSS_COMPILE)$(MCC)
GCC_         = $(CROSS_COMPILE)gcc
CFLAGS_      = $(CFLAGS) -O3 -std=gnu99 -funsafe-math-optimizations
MCC_FLAGS_   = $(MCC_FLAGS) --ompss -DRUNTIME_MODE=\"perf\"
MCC_FLAGS_I_ = $(MCC_FLAGS_) --instrument -DRUNTIME_MODE=\"instr\"
MCC_FLAGS_D_ = $(MCC_FLAGS_) --debug -g -k -DRUNTIME_MODE=\"debug\"
LDFLAGS_     = $(LDFLAGS) -lm

# FPGA bitstream Variables
FPGA_HWRUNTIME         ?= som
FPGA_CLOCK             ?= 200
FPGA_MEMORY_PORT_WIDTH ?= 128
INTERCONNECT_OPT       ?= performance
NBODY_BLOCK_SIZE       ?= 2048
NBODY_NCALCFORCES      ?= 8
NBODY_NUM_FBLOCK_ACCS  ?= 1

CFLAGS_ += -DNBODY_BLOCK_SIZE=$(NBODY_BLOCK_SIZE) -DNBODY_NCALCFORCES=$(NBODY_NCALCFORCES) -DNBODY_NUM_FBLOCK_ACCS=$(NBODY_NUM_FBLOCK_ACCS) -DFPGA_HWRUNTIME=\"$(FPGA_HWRUNTIME)\" -DFPGA_MEMORY_PORT_WIDTH=$(FPGA_MEMORY_PORT_WIDTH)
FPGA_LINKER_FLAGS_ =--Wf,--name=$(PROGRAM_),--board=$(BOARD),-c=$(FPGA_CLOCK),--hwruntime=$(FPGA_HWRUNTIME),--from_step=$(FROM_STEP),--to_step=$(TO_STEP)
ifdef FPGA_MEMORY_PORT_WIDTH
	MCC_FLAGS_ += --variable=fpga_memory_port_width:$(FPGA_MEMORY_PORT_WIDTH)
endif
ifdef MEMORY_INTERLEAVING_STRIDE
	FPGA_LINKER_FLAGS_ += --Wf,--memory_interleaving_stride=$(MEMORY_INTERLEAVING_STRIDE)
endif
ifdef SIMPLIFY_INTERCONNECTION
	FPGA_LINKER_FLAGS_ += --Wf,--simplify_interconnection
endif
ifdef INTERCONNECT_OPT
	FPGA_LINKER_FLAGS_ += --Wf,--interconnect_opt=$(INTERCONNECT_OPT)
endif
ifdef INTERCONNECT_REGSLICE
	FPGA_LINKER_FLAGS_ += --Wf,--interconnect_regslice,$(INTERCONNECT_REGSLICE)
endif
ifeq ($(FPGA_HWRUNTIME),pom)
	FPGA_LINKER_FLAGS_ += --Wf,--picos_max_deps_per_task=3,--picos_max_args_per_task=11,--picos_max_copies_per_task=11,--picos_tm_size=32,--picos_dm_size=102,--picos_vm_size=102
endif

help:
	@echo 'Supported targets:       $(PROGRAM_)-p, $(PROGRAM_)-i, $(PROGRAM_)-d, $(PROGRAM_)-seq, design-p, design-i, design-d, bitstream-p, bitstream-i, bitstream-d, clean, help'
	@echo 'Environment variables:   CFLAGS, CROSS_COMPILE, LDFLAGS, MCC, MCC_FLAGS'
	@echo 'FPGA env. variables:     BOARD, FPGA_HWRUNTIME, FPGA_CLOCK, FPGA_MEMORY_PORT_WIDTH, NBODY_BLOCK_SIZE, NBODY_NCALCFORCES, NBODY_NUM_FBLOCK_ACCS'

$(PROGRAM_)-p: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-i: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_I_) $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-d: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_D_) $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-seq: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(GCC_) $(CFLAGS_) -DRUNTIME_MODE=\"seq\" $^ -o $@ $(LDFLAGS_)

design-p: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		--Wf,--to_step=design \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

design-i: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_I_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		--Wf,--to_step=design \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

design-d: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_D_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		--Wf,--to_step=design,--debug_intfs=both \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

bitstream-p: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

bitstream-i: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_I_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

bitstream-d: ./src/$(PROGRAM_).c ./src/kernel_$(FPGA_HWRUNTIME).c
	$(eval TMPFILE := $(shell mktemp))
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_D_) --bitstream-generation $(FPGA_LINKER_FLAGS_) \
		--Wf,--debug_intfs=both \
		$^ -o $(TMPFILE) $(LDFLAGS_)
	rm $(TMPFILE)

clean:
	rm -fv *.o $(PROGRAM_)-? $(MCC_)_$(PROGRAM_)*.c *_ompss.cpp ait_$(PROGRAM_)*.json
	rm -fr $(PROGRAM_)_ait
