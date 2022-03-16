/*
 * Copyright 2013-2019 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <sys/time.h>
#include <stdlib.h>
#include <strings.h>

#include "srslte/phy/io/binsource.h"
#include "srslte/phy/utils/bit.h"

#define DIV(a,b) ((a-1)/b+1)


/* Internal functions */
static int gen_seq_buff(srslte_binsource_t* q, int nwords) {
  if (q->seq_buff_nwords != nwords) {
    free(q->seq_buff);
    q->seq_buff_nwords = 0;
  }
  if (!q->seq_buff_nwords) {
    q->seq_buff = malloc(nwords*sizeof(uint32_t));
    if (!q->seq_buff) {
      return -1;
    }
    q->seq_buff_nwords = nwords;
  }
  for (int i=0;i<q->seq_buff_nwords;i++) {
    q->seq_buff[i] = rand_r(&q->seed);
  }
  return 0;
}

/* Low-level API */

/**
 * Initializes the binsource object.
 */
void srslte_binsource_init(srslte_binsource_t* q) {
  bzero((void*) q,sizeof(srslte_binsource_t));
}

/**
 * Destroys binsource object
 */
void srslte_binsource_free(srslte_binsource_t* q) {
  if (q->seq_buff) {
    free(q->seq_buff);
  }
  bzero(q, sizeof(srslte_binsource_t));
}

/**
 * Sets a new seed
 */
void srslte_binsource_seed_set(srslte_binsource_t* q, uint32_t seed) {
  q->seed = seed;
}

/**
 * Sets local time as seed.
 */
void srslte_binsource_seed_time(srslte_binsource_t *q) {
  struct timeval t1;
  gettimeofday(&t1, NULL);
  q->seed = t1.tv_usec * t1.tv_sec;
}

/**
 * Generates a sequence of nbits random bits
 */
int srslte_binsource_cache_gen(srslte_binsource_t* q, int nbits) {
  if (gen_seq_buff(q,DIV(nbits,32))) {
    return -1;
  }
  q->seq_cache_nbits = nbits;
  q->seq_cache_rp = 0;
  return 0;
}

static int int_2_bits(uint32_t* src, uint8_t* dst, int nbits) {
  int n;
  n=nbits/32;
  for (int i=0;i<n;i++) {
    srslte_bit_unpack(src[i],&dst,32);
  }
  srslte_bit_unpack(src[n],&dst,nbits-n*32);
  return n;
}

/**
 * Copies the next random bits to the buffer bits from the array generated by srslte_binsource_cache_gen
 */
void srslte_binsource_cache_cpy(srslte_binsource_t* q, uint8_t *bits, int nbits) {
  q->seq_cache_rp += int_2_bits(&q->seq_buff[q->seq_cache_rp],bits,nbits);
}

/**
 * Stores in the bits buffer a sequence of nbits pseudo-random bits.
 * Overwrites the bits generated using srslte_binsource_cache_gen.
 */
int srslte_binsource_generate(srslte_binsource_t* q, uint8_t *bits, int nbits) {

  if (gen_seq_buff(q,DIV(nbits,32))) {
    return -1;
  }
  int_2_bits(q->seq_buff,bits,nbits);
  return 0;
}



