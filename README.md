# N-Body

**Name**: N-Body Simulation
**Contact Person**: OmpSs@FPGA Team, ompss-fpga-support@bsc.es  
**License Agreement**: GPL  
**Platform**: OmpSs@FPGA

### Description
N-body simulation is a simulation of a dynamical system of particles, usually under the influence of physical forces, such as gravity.

### Build instructions
Clone the repository:
```
git clone https://github.com/bsc-pm-ompss-at-fpga/N-body.git
cd nbody
```

Build the application binaries:
###### ARM32
```
make BOARD=zedboard CROSS_COMPILE=arm-linux-gnueabihf-
```

###### ARM64
```
make BOARD=zcu102 CROSS_COMPILE=aarch64-linux-gnu-
```

###### x86
```
make BOARD=alveo_u200
```

##### Build variables
You can change the build process defining or modifying some environment variables.
The supported ones are:
  - `CFLAGS`
  - `LDFLAGS`
  - `MCC`. If not defined, the default value is: `fpgacc`
  - `CROSS_COMPILE`
  - `BOARD`. Board option used when generating the bitstreams.
  - `FPGA_HWRUNTIME`. Hardware runtime to use in the bitstreams. The default value is: `som`
  - `FPGA_CLOCK`. Target frequency of FPGA accelerators in the bitstreams. The default value is: `200`.
  - `FPGA_MEMORY_PORT_WIDTH`. Bit-width of accelerators memory port to access main memory. The default value is: `128`.
  - `NBODY_BLOCK_SIZE`. Number of particles that FPGA accelerators deal with. The default value is: `2048`.
  - `NBODY_NCALCFORCES`. Number of forces calculated in parallel in each FPGA task accelerator. The default value is: `8`.
  - `NBODY_NUM_FBLOCK_ACCS`. Number of FPGA accelerators for calculate_forces_BLOCK task. The default value is: `1`.

### Run instructions
The name of each binary file created by build step ends with a suffix which determines the version:
 - program-p: performance version
 - program-i: instrumented version
 - program-d: debug version

All versions expect two execution arguments defining the number of particles and the number of timesteps.
A third optional argument can be used to enable the silent mode.
```
USAGE: ./nbody-p <num particles> <timesteps> [<silent mode>]
```
