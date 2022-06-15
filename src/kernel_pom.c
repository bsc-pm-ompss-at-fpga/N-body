/*
* Copyright (c) 2020-2022, Barcelona Supercomputing Center
*                          Centro Nacional de Supercomputacion
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

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "kernel.h"
#include "kernel.fpga.h"

extern double wall_time(void);

void calculate_forces_part(
      const float pos_x1, const float pos_y1, const float pos_z1, const float mass1,
      const float pos_x2, const float pos_y2, const float pos_z2, const float weight2,
      float * fx, float * fy, float * fz)
{
   #pragma HLS inline
   const float local_x = *fx;
   const float local_y = *fy;
   const float local_z = *fz;

   const float diff_x = pos_x2 - pos_x1;
   const float diff_y = pos_y2 - pos_y1;
   const float diff_z = pos_z2 - pos_z1;

   const float distance_squared = diff_x * diff_x + diff_y * diff_y + diff_z * diff_z;
   const float distance = sqrtf(distance_squared);

   const float force = mass1 / (distance_squared * distance) * weight2;
   const float force_corrected = distance_squared == 0.0f ? 0.0f : force;

   *fx = local_x + force_corrected * diff_x;
   *fy = local_y + force_corrected * diff_y;
   *fz = local_z + force_corrected * diff_z;
}

#pragma omp target device(fpga) num_instances(FBLOCK_NUM_ACCS) localmem_copies no_copy_deps \
  copy_inout([BLOCK_SIZE]x, [BLOCK_SIZE]y, [BLOCK_SIZE]z) \
  copy_in([BLOCK_SIZE]pos_x1, [BLOCK_SIZE]pos_y1, [BLOCK_SIZE]pos_z1, [BLOCK_SIZE]mass1) \
  copy_in([BLOCK_SIZE]pos_x2, [BLOCK_SIZE]pos_y2, [BLOCK_SIZE]pos_z2, [BLOCK_SIZE]weight2)
#pragma omp task label(calculate_forces_BLOCK) inout([FORCE_FPGABLOCK_SIZE]x) in(pos_x1[0], pos_y2[0])
void calculate_forces_BLOCK(float *x, float *y, float *z,
   const float *pos_x1, const float *pos_y1, const float *pos_z1, const float *mass1,
   const float *pos_x2, const float *pos_y2, const float *pos_z2, const float *weight2)
{
   #pragma HLS inline
   //NOTE: Partition in a way that we can read/write enough data each cycle
   #pragma HLS array_partition variable=x cyclic factor=NCALCFORCES
   #pragma HLS array_partition variable=y cyclic factor=NCALCFORCES
   #pragma HLS array_partition variable=z cyclic factor=NCALCFORCES
   #pragma HLS array_partition variable=pos_x1 cyclic factor=NCALCFORCES/2
   #pragma HLS array_partition variable=pos_y1 cyclic factor=NCALCFORCES/2
   #pragma HLS array_partition variable=pos_z1 cyclic factor=NCALCFORCES/2
   #pragma HLS array_partition variable=mass1  cyclic factor=NCALCFORCES/2
   #pragma HLS array_partition variable=pos_x2 cyclic factor=FPGA_PWIDTH/64
   #pragma HLS array_partition variable=pos_y2 cyclic factor=FPGA_PWIDTH/64
   #pragma HLS array_partition variable=pos_z2 cyclic factor=FPGA_PWIDTH/64
   #pragma HLS array_partition variable=weight2  cyclic factor=FPGA_PWIDTH/64

   int i, j;
   for (j = 0; j < BLOCK_SIZE; j++) {
      for (i = 0; i < BLOCK_SIZE; i++) {
         #pragma HLS pipeline II=1
         #pragma HLS unroll factor=NCALCFORCES

         calculate_forces_part(
               pos_x1[i], pos_y1[i], pos_z1[i], mass1[i],
               pos_x2[j], pos_y2[j], pos_z2[j], weight2[j],
               &x[i], &y[i], &z[i]
         );
      }
   }
}

void calculate_forces(const int n_blocks, float * forces, const float * particles)
{
   #pragma HLS inline
   int j, i;
   for (j = 0; j < n_blocks; j++) {
      for (i = 0; i < n_blocks; i++) {
         float * forcesTarget = forces + i*FORCE_FPGABLOCK_SIZE;
         const float * block1 = particles + i*PARTICLES_FPGABLOCK_SIZE;
         const float * block2 = particles + j*PARTICLES_FPGABLOCK_SIZE;

         calculate_forces_BLOCK(
               forcesTarget + FORCE_FPGABLOCK_X_OFFSET, forcesTarget + FORCE_FPGABLOCK_Y_OFFSET,
               forcesTarget + FORCE_FPGABLOCK_Z_OFFSET, block1 + PARTICLES_FPGABLOCK_POS_X_OFFSET,
               block1 + PARTICLES_FPGABLOCK_POS_Y_OFFSET, block1 + PARTICLES_FPGABLOCK_POS_Z_OFFSET,
               block1 + PARTICLES_FPGABLOCK_MASS_OFFSET, block2 + PARTICLES_FPGABLOCK_POS_X_OFFSET,
               block2 + PARTICLES_FPGABLOCK_POS_Y_OFFSET, block2 + PARTICLES_FPGABLOCK_POS_Z_OFFSET,
               block2 + PARTICLES_FPGABLOCK_WEIGHT_OFFSET);
      }
   }
}

#pragma omp target device(fpga) localmem_copies no_copy_deps \
  copy_inout([PARTICLES_FPGABLOCK_SIZE]particles, [FORCE_FPGABLOCK_SIZE]forces)
#pragma omp task label(update_partices_BLOCK) inout(particles[PARTICLES_FPGABLOCK_POS_X_OFFSET], particles[PARTICLES_FPGABLOCK_POS_Y_OFFSET])
void update_particles_BLOCK(float * particles, float * forces, const float time_interval)
{
    #pragma HLS inline
    #pragma HLS array_partition variable=forces cyclic factor=FPGA_PWIDTH/64
    #pragma HLS array_partition variable=particles cyclic factor=FPGA_PWIDTH/64

   int e;
   for (e=0; e < BLOCK_SIZE; e++) {
      //There are 7 loads to the particles array which can't be done in the same cycle
      #pragma HLS pipeline II=7
      #pragma HLS dependence variable=particles inter false
      #pragma HLS dependence variable=forces inter false

      const float mass       = particles[PARTICLES_FPGABLOCK_MASS_OFFSET + e];
      const float velocity_x = particles[PARTICLES_FPGABLOCK_VEL_X_OFFSET + e];
      const float velocity_y = particles[PARTICLES_FPGABLOCK_VEL_Y_OFFSET + e];
      const float velocity_z = particles[PARTICLES_FPGABLOCK_VEL_Z_OFFSET + e];

      const float position_x = particles[PARTICLES_FPGABLOCK_POS_X_OFFSET + e];
      const float position_y = particles[PARTICLES_FPGABLOCK_POS_Y_OFFSET + e];
      const float position_z = particles[PARTICLES_FPGABLOCK_POS_Z_OFFSET + e];

      const float time_by_mass       = time_interval / mass;
      const float half_time_interval = 0.5f * time_interval;

      const float velocity_change_x = forces[FORCE_FPGABLOCK_X_OFFSET + e] * time_by_mass;
      const float velocity_change_y = forces[FORCE_FPGABLOCK_Y_OFFSET + e] * time_by_mass;
      const float velocity_change_z = forces[FORCE_FPGABLOCK_Z_OFFSET + e] * time_by_mass;

      const float position_change_x = velocity_x + velocity_change_x * half_time_interval;
      const float position_change_y = velocity_y + velocity_change_y * half_time_interval;
      const float position_change_z = velocity_z + velocity_change_z * half_time_interval;

      particles[PARTICLES_FPGABLOCK_VEL_X_OFFSET + e] = velocity_x + velocity_change_x;
      particles[PARTICLES_FPGABLOCK_VEL_Y_OFFSET + e] = velocity_y + velocity_change_y;
      particles[PARTICLES_FPGABLOCK_VEL_Z_OFFSET + e] = velocity_z + velocity_change_z;

      particles[PARTICLES_FPGABLOCK_POS_X_OFFSET + e] = position_x + position_change_x;
      particles[PARTICLES_FPGABLOCK_POS_Y_OFFSET + e] = position_y + position_change_y;
      particles[PARTICLES_FPGABLOCK_POS_Z_OFFSET + e] = position_z + position_change_z;

      forces[FORCE_FPGABLOCK_X_OFFSET + e] = 0.0f;
      forces[FORCE_FPGABLOCK_Y_OFFSET + e] = 0.0f;
      forces[FORCE_FPGABLOCK_Z_OFFSET + e] = 0.0f;
   }
}

void update_particles(const int n_blocks, float * particles,
      float * forces, const float time_interval)
{
   #pragma HLS inline
   int i;
   for (i = 0; i < n_blocks; i++) {
      update_particles_BLOCK(particles + i*PARTICLES_FPGABLOCK_SIZE, forces + i*FORCE_FPGABLOCK_SIZE, time_interval);
   }
}

void solve_nbody(float * particles, float * forces, const int n_blocks,
      const int timesteps, const float time_interval )
{
   #pragma HLS inline
   int t, i, j;
   for(t = 0; t < timesteps; t++) {
      calculate_forces(n_blocks, forces, particles);

      update_particles(n_blocks, particles, forces, time_interval);
   }
}

#pragma omp target device(fpga) copy_inout([n_blocks*PARTICLES_FPGABLOCK_SIZE]particles, [n_blocks*FORCE_FPGABLOCK_SIZE]forces)
#pragma omp task label(solve_nbody_task)
void solve_nbody_task(float * particles, float * forces, const int n_blocks,
      const int timesteps, const float time_interval )
{
   solve_nbody(particles, forces, n_blocks, timesteps, time_interval);
   #pragma omp taskwait
}

void solve_nbody_wrapper(particles_block_t * __restrict__ particles, force_block_t * __restrict__ forces,
      const int n_blocks, const int timesteps, const float time_interval, double *times )
{
   times[0] = wall_time();

   float * particles_fpga = (float *)particles;
   float * forces_fpga = (float *)forces;
   times[1] = wall_time();

   solve_nbody_task((float *)particles_fpga, (float *)forces_fpga, n_blocks, timesteps, time_interval);
   #pragma omp taskwait noflush
   times[2] = wall_time();

   #pragma omp taskwait
   times[3] = wall_time();
}
