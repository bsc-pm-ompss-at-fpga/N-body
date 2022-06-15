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

#ifndef nbody_h
#define nbody_h

#include <stddef.h>
#include "kernel.h"

static inline int log2i (int i) {
	return 31-__builtin_clz(i);
}

typedef struct {
   float x; /* x */
   float y; /* y */
   float z; /* z */
} single_force;

#define PAGE_SIZE 4096
#define MIN_PARTICLES (4096*BLOCK_SIZE/sizeof(particles_block_t))
#define PRECISION 0.000001

#define roundup(x, y) (                                 \
{                                                       \
        const typeof(y) __y = y;                        \
        (((x) + (__y - 1)) / __y) * __y;                \
}                                                       \
)

static const float default_domain_size_x = 1.0e+6; /* m  */
static const float default_domain_size_y = 1.0e+6; /* m  */
static const float default_domain_size_z = 1.0e+6; /* m  */
static const float default_mass_maximum  = 1.0e+10; /* kg */
static const float default_time_interval = 1.0e+0;  /* s  */
static const int   default_seed          = 12345;
static const char* default_name          = "particles";

typedef struct {
   size_t total_size;
   size_t size;
   size_t offset;
   char name[1000];
} nbody_file_t;

typedef const struct {
   float domain_size_x;
   float domain_size_y;
   float domain_size_z;
   float mass_maximum;
   float time_interval;
   int   seed;
   const char* name;
   const int timesteps;
   const int   num_particles;
} nbody_conf_t;

typedef const struct {
   particle_ptr_t const local;
   particle_ptr_t const remote;
   force_ptr_t    const forces;
   const int num_particles;
   const int timesteps;
   nbody_file_t file;
} nbody_t;

/* coomon.c */
nbody_t nbody_setup(nbody_conf_t * const conf);
void nbody_save_particles(nbody_t *nbody, const int timesteps);
void nbody_free(nbody_t *nbody);
int nbody_check(nbody_t *nbody, const int timesteps);

double wall_time(void);

void print_stats(double n_blocks, int timesteps, double elapsed_time);

void exchange_particles(particles_block_t * const sendbuf, particles_block_t * recvbuf, const int n_blocks,
                        const int rank, const int rank_size, const int i);

#endif /* #ifndef nbody_h */
