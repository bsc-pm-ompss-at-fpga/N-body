/*
* Copyright (c) 2020, Barcelona Supercomputing Center
*                     Centro Nacional de Supercomputacion
*
* This program is free software: you can redistribute it and/or modify  
* it under the terms of the GNU General Public License as published by  
* the Free Software Foundation, version 3.
*
* This program is distributed in the hope that it will be useful, but 
* WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License 
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __KERNEL_H__
#define __KERNEL_H__

#ifndef NBODY_BLOCK_SIZE
# error NBODY_BLOCK_SIZE is not defined
#endif
#ifndef NBODY_NCALCFORCES
# error NBODY_NCALCFORCES is not defined
#endif

static const float gravitational_constant =  6.6726e-11; /* N(m/kg)2 */
static const unsigned int BLOCK_SIZE = NBODY_BLOCK_SIZE;
#pragma omp target device(fpga)
static const unsigned int NCALCFORCES = NBODY_NCALCFORCES;
#pragma omp target device(fpga)
const unsigned int FPGA_PWIDTH = FPGA_MEMORY_PORT_WIDTH;
static const unsigned int FBLOCK_NUM_ACCS = NBODY_NUM_FBLOCK_ACCS;

static const unsigned int PARTICLES_FPGABLOCK_POS_X_OFFSET  = 0*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_POS_Y_OFFSET  = 1*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_POS_Z_OFFSET  = 2*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_VEL_X_OFFSET  = 3*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_VEL_Y_OFFSET  = 4*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_VEL_Z_OFFSET  = 5*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_MASS_OFFSET   = 6*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_WEIGHT_OFFSET = 7*NBODY_BLOCK_SIZE;
static const unsigned int PARTICLES_FPGABLOCK_SIZE          = 8*NBODY_BLOCK_SIZE;

static const unsigned int FORCE_FPGABLOCK_X_OFFSET = 0*NBODY_BLOCK_SIZE;
static const unsigned int FORCE_FPGABLOCK_Y_OFFSET = 1*NBODY_BLOCK_SIZE;
static const unsigned int FORCE_FPGABLOCK_Z_OFFSET = 2*NBODY_BLOCK_SIZE;
static const unsigned int FORCE_FPGABLOCK_SIZE     = 3*NBODY_BLOCK_SIZE;

typedef struct {
   float position_x[NBODY_BLOCK_SIZE]; /* m   */
   float position_y[NBODY_BLOCK_SIZE]; /* m   */
   float position_z[NBODY_BLOCK_SIZE]; /* m   */
   float velocity_x[NBODY_BLOCK_SIZE]; /* m/s */
   float velocity_y[NBODY_BLOCK_SIZE]; /* m/s */
   float velocity_z[NBODY_BLOCK_SIZE]; /* m/s */
   float mass[NBODY_BLOCK_SIZE];       /* kg  */
   float weight[NBODY_BLOCK_SIZE];
} particles_block_t;

typedef particles_block_t * __restrict__ const particle_ptr_t;

typedef struct {
   float x[NBODY_BLOCK_SIZE]; /* x */
   float y[NBODY_BLOCK_SIZE]; /* y */
   float z[NBODY_BLOCK_SIZE]; /* z */
} force_block_t;

typedef force_block_t * __restrict__ const force_ptr_t;

void solve_nbody_wrapper(particles_block_t * __restrict__ particles, force_block_t * __restrict__ forces,
      const int n_blocks, const int timesteps, const float time_interval, double * times );

#endif //__KERNEL_H__
