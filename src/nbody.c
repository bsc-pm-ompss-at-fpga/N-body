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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>
#include <ieee754.h>
#include <time.h>
#include "nbody.h"

int silent;

void particle_init(nbody_conf_t * const conf, particles_block_t* const part)
{
   int i;
   for (i=0;i < BLOCK_SIZE;i++) {
      part->position_x[i] = conf->domain_size_x * ((float) random() / ((float)RAND_MAX /*+ 1.0*/));
      part->position_y[i] = conf->domain_size_y * ((float) random() / ((float)RAND_MAX /*+ 1.0*/));
      part->position_z[i] = conf->domain_size_z * ((float) random() / ((float)RAND_MAX /*+ 1.0*/));
      part->mass[i]       = conf->mass_maximum  * ((float) random() / ((float)RAND_MAX /*+ 1.0*/));
      part->weight[i]     = gravitational_constant * part->mass[i];
   }
}

void nbody_generate_particles(nbody_conf_t * conf, nbody_file_t * file)
{
   int i;
   char fname[1024];
   sprintf(fname, "%s.in", file->name);

   if( access( fname, F_OK ) == 0 ) return;

   const int fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH);
   assert(fd >= 0);

   assert(file->total_size % PAGE_SIZE == 0);
   assert(ftruncate(fd, file->total_size) == 0);

   particles_block_t * const particles = mmap(NULL, file->total_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);

   const int total_num_particles = file->total_size / sizeof(particles_block_t);

   for(i=0; i<total_num_particles; i++){
      particle_init(conf, particles+i);
   }

   assert(munmap(particles, file->total_size) == 0);
   close(fd);
}

int compare_positions(const float p0, const float p1)
{
   return p0 > p1*(1.0 + PRECISION) ? 1 : ( p0 < p1*(1.0 - PRECISION) ? -1 : 0 );
}

int nbody_check(const nbody_t *nbody, const int timesteps)
{
   char fname[1024];
   sprintf(fname, "./input/%s.ref", nbody->file.name);

   if ( access( fname, F_OK ) != 0 ) return 0;

   const int fd = open (fname, O_RDONLY, 0);
   assert(fd >= 0);

   particle_ptr_t particles = mmap(NULL, nbody->file.total_size, PROT_READ, MAP_SHARED, fd, nbody->file.offset);

   double error = 0.0;
   int i, e, count = 0;
   for(i=0; i< nbody->num_particles; i++){

      for (e=0;e < BLOCK_SIZE;e++){

         if(nbody->local[i].position_x[e] != particles[i].position_x[e] ||
               nbody->local[i].position_y[e] != particles[i].position_y[e] ||
               nbody->local[i].position_z[e] != particles[i].position_z[e])
         {
            error += fabs(((nbody->local[i].position_x[e] - particles[i].position_x[e])*100.0)/particles[i].position_x[e]) +
               fabs(((nbody->local[i].position_y[e] - particles[i].position_y[e])*100.0)/particles[i].position_y[e]) +
               fabs(((nbody->local[i].position_z[e] - particles[i].position_z[e])*100.0)/particles[i].position_z[e]);
            count++;
         }
      }
   }
   double relative_error = error/(3.0*count);

   if((count*100.0)/(nbody->num_particles*BLOCK_SIZE) > 0.6 || (relative_error >  0.000008)) {
      silent?:printf("> Relative error[%d]: %f\n", count, relative_error);
      return -1;
   } else {
      silent?:printf("> Result validation: OK\n");
      return 1;
   }
}

particles_block_t * nbody_load_particles(nbody_conf_t * conf, nbody_file_t * file)
{

   char fname[1024];
   sprintf(fname, "%s.in", file->name);

   const int fd = open (fname, O_RDONLY, 0);
   assert(fd >= 0);

   void * const ptr = mmap(NULL, file->size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, file->offset);
   assert(ptr != MAP_FAILED);

   assert(close(fd) == 0);

   return ptr;
}

void * nbody_alloc(const size_t size)
{
   void * const space = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   assert(space != MAP_FAILED);
   return space;
}

force_ptr_t nbody_alloc_forces(nbody_conf_t * const conf)
{
   return nbody_alloc(conf->num_particles*sizeof(force_block_t));
}

particle_ptr_t nbody_alloc_particles(nbody_conf_t * const conf)
{
   return nbody_alloc(conf->num_particles*sizeof(particles_block_t));
}

nbody_file_t nbody_setup_file(nbody_conf_t * const conf)
{
#if 0
   int rank, rank_size;
   assert(MPI_Comm_size(MPI_COMM_WORLD, &rank_size) == MPI_SUCCESS);
   assert(MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS);
#else
   int rank = 0, rank_size = 1;
#endif

   nbody_file_t file;

   const int total_num_particles = conf->num_particles*rank_size;

   file.total_size = total_num_particles*sizeof(particles_block_t);
   file.size   = file.total_size/rank_size;
   file.offset = file.size*rank;

   sprintf(file.name, "%s-%d-%d-%d", conf->name, BLOCK_SIZE*total_num_particles, BLOCK_SIZE, conf->timesteps);

   return file;
}

nbody_t nbody_setup(nbody_conf_t * const conf)
{

   nbody_file_t file = nbody_setup_file(conf);

   if (file.offset == 0) nbody_generate_particles(conf, &file);

   nbody_t nbody = {
      nbody_load_particles(conf, &file),
      nbody_alloc_particles(conf),
      nbody_alloc_forces(conf),
      conf->num_particles,
      conf->timesteps,
      file
   };

   return nbody;
}

void nbody_save_particles(nbody_t * const nbody, const int timesteps)
{
   char fname[1024];
   sprintf(fname, "%s.out", nbody->file.name);

   const int fd = open (fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
   assert(fd >= 0);

   write(fd, nbody->local, nbody->file.total_size);

   assert(close(fd) == 0);

}

void nbody_free(nbody_t * const nbody)
{
   {
      const size_t size = nbody->num_particles*sizeof(particles_block_t);
      assert(munmap(nbody->local, size) == 0);
   }
   {
      const size_t size = nbody->num_particles*sizeof(force_block_t);
      assert(munmap(nbody->forces, size) == 0);
   }
}

double wall_time(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC,&ts);
   return (double) (ts.tv_sec)  + (double) ts.tv_nsec * 1.0e-9;
}

int main(int argc, char** argv)
{
   if (argc < 3 || argc > 3) {
      fprintf(stderr, "USAGE: %s <num particles> <timesteps>\n", argv[0]);
      return 1;
   }

   int num_particles   = roundup(atoi(argv[1]), MIN_PARTICLES);
   const int timesteps = atoi(argv[2]);

   assert(timesteps > 0);
   assert(num_particles >= 4096);

   num_particles = num_particles/BLOCK_SIZE;

   nbody_conf_t conf = { default_domain_size_x, default_domain_size_y, default_domain_size_z,
                         default_mass_maximum, default_time_interval, default_seed, default_name,
                         timesteps /* arg */, num_particles /* arg */ };

   nbody_t nbody = nbody_setup( &conf );

   double times[4];
   solve_nbody_wrapper(nbody.local, nbody.forces, num_particles, timesteps, conf.time_interval, times);

   nbody_save_particles(&nbody, timesteps);
   int result = nbody_check(&nbody, timesteps);
   nbody_free(&nbody);

   double throughput = (double)(num_particles * BLOCK_SIZE) * (double)(num_particles * BLOCK_SIZE ) / 1.0E9;
   throughput = throughput * (double)timesteps / (times[2] - times[1]);

   const char * check[] = {"fail","n/a","successful"};

   int check_idx;

   if ( result <  0 ) check_idx = 0;
   if ( result == 0 ) check_idx = 1;
   if ( result >  0 ) check_idx = 2;

   // Print results
   printf( "==================== RESULTS ===================== \n" );
   printf( "  Benchmark: %s (%s)\n", "N-Body", "OmpSs");
   printf( "  Total particles: %d\n", num_particles * BLOCK_SIZE );
   printf( "  Timesteps: %d\n", timesteps );
   printf( "  Verification: %s\n", check[check_idx] );
   printf( "  Warm up time (secs): %f\n", times[1] - times[0]);
   printf( "  Execution time (secs): %f\n", times[2] - times[1]);
   printf( "  Flush time (secs): %f\n", times[3] - times[2]);
   printf( "  Throughput (gpairs/s): %f\t\n", throughput);
   printf( "================================================== \n" );

   //Create the JSON result file
   FILE *res_file = fopen("test_result.json", "w+");
   if (res_file == NULL) {
      printf( "Cannot open 'test_result.json' file\n" );
      exit(1);
   }
   fprintf(res_file,
      "{ \
         \"benchmark\": \"%s\", \
         \"version\": \"%daccs 250mhz memport_128 noflush fpga_solve_tw\", \
         \"hwruntime\": \"%s\", \
         \"pm\": \"%s_%s\", \
         \"datatype\": \"%s\", \
         \"argv\": \"%d %d %d\", \
         \"exectime\": \"%f\", \
         \"performance\": \"%f\", \
         \"note\": \"warm %f, exec %f, flush %f\" \
      }",
      "nbody",
      FBLOCK_NUM_ACCS,
      FPGA_HWRUNTIME,
      "ompss", RUNTIME_MODE,
      "float",
      num_particles * BLOCK_SIZE, BLOCK_SIZE, timesteps,
      times[2] - times[1],
      throughput,
      times[1] - times[0],
      times[2] - times[1],
      times[3] - times[2]
   );
   fclose(res_file);

   return result < 0;
}
