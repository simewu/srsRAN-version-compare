/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
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

#include <stdlib.h>
#include <complex.h>
#include <string.h>
#include <math.h>

#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/mimo/precoding.h"
#include "srslte/phy/utils/vector.h"
#include "srslte/phy/utils/debug.h"
#include "srslte/phy/utils/mat.h"

#ifdef LV_HAVE_SSE
#include <immintrin.h>
int srslte_predecoding_single_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x, int nof_rxant, int nof_symbols, float scaling, float noise_estimate);
int srslte_predecoding_diversity2_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS], int nof_rxant, int nof_symbols, float scaling);
#endif

#ifdef LV_HAVE_AVX
#include <immintrin.h>
int srslte_predecoding_single_avx(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x, int nof_rxant, int nof_symbols, float scaling, float noise_estimate);
#endif
#include "srslte/phy/utils/mat.h"

static srslte_mimo_decoder_t mimo_decoder = SRSLTE_MIMO_DECODER_MMSE;

/************************************************
 * 
 * RECEIVER SIDE FUNCTIONS
 * 
 **************************************************/

#ifdef LV_HAVE_SSE

#define PROD(a,b) _mm_addsub_ps(_mm_mul_ps(a,_mm_moveldup_ps(b)),_mm_mul_ps(_mm_shuffle_ps(a,a,0xB1),_mm_movehdup_ps(b)))

int srslte_predecoding_single_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x, int nof_rxant, int nof_symbols, float scaling, float noise_estimate) {
  
  float *xPtr = (float*) x;
  const float *hPtr1 = (const float*) h[0];
  const float *yPtr1 = (const float*) y[0];
  const float *hPtr2 = (const float*) h[1];
  const float *yPtr2 = (const float*) y[1];

  __m128 conjugator = _mm_setr_ps(0, -0.f, 0, -0.f);
  
  __m128 noise = _mm_set1_ps(noise_estimate);
  __m128 h1Val1, h2Val1, y1Val1, y2Val1;
  __m128 h1Val2, h2Val2, y1Val2, y2Val2;
  __m128 hsquare, h1square, h2square, h1conj1, h2conj1, x1Val1, x2Val1;
  __m128 hsquare2, h1conj2, h2conj2, x1Val2, x2Val2;

  for (int i=0;i<nof_symbols/4;i++) {
    y1Val1 = _mm_load_ps(yPtr1); yPtr1+=4;
    y2Val1 = _mm_load_ps(yPtr1); yPtr1+=4;
    h1Val1 = _mm_load_ps(hPtr1); hPtr1+=4;
    h2Val1 = _mm_load_ps(hPtr1); hPtr1+=4;

    if (nof_rxant == 2) {
      y1Val2 = _mm_load_ps(yPtr2); yPtr2+=4;
      y2Val2 = _mm_load_ps(yPtr2); yPtr2+=4;
      h1Val2 = _mm_load_ps(hPtr2); hPtr2+=4;
      h2Val2 = _mm_load_ps(hPtr2); hPtr2+=4;      
    }
    
    hsquare = _mm_hadd_ps(_mm_mul_ps(h1Val1, h1Val1), _mm_mul_ps(h2Val1, h2Val1)); 
    if (nof_rxant == 2) {
      hsquare2 = _mm_hadd_ps(_mm_mul_ps(h1Val2, h1Val2), _mm_mul_ps(h2Val2, h2Val2)); 
      hsquare = _mm_add_ps(hsquare, hsquare2);
    }
    if (noise_estimate > 0) {
      hsquare  = _mm_add_ps(hsquare, noise);
    }
    
    h1square  = _mm_shuffle_ps(hsquare, hsquare, _MM_SHUFFLE(1, 1, 0, 0));
    h2square  = _mm_shuffle_ps(hsquare, hsquare, _MM_SHUFFLE(3, 3, 2, 2));
    
    /* Conjugate channel */
    h1conj1 = _mm_xor_ps(h1Val1, conjugator); 
    h2conj1 = _mm_xor_ps(h2Val1, conjugator); 

    if (nof_rxant == 2) {
      h1conj2 = _mm_xor_ps(h1Val2, conjugator); 
      h2conj2 = _mm_xor_ps(h2Val2, conjugator); 
    }
    
    /* Complex product */      
    x1Val1 = PROD(y1Val1, h1conj1);
    x2Val1 = PROD(y2Val1, h2conj1);

    if (nof_rxant == 2) {
      x1Val2 = PROD(y1Val2, h1conj2);
      x2Val2 = PROD(y2Val2, h2conj2);
      x1Val1 = _mm_add_ps(x1Val1, x1Val2);
      x2Val1 = _mm_add_ps(x2Val1, x2Val2);
    }
    
    x1Val1 = _mm_div_ps(x1Val1, h1square);
    x2Val1 = _mm_div_ps(x2Val1, h2square);

    x1Val1 = _mm_mul_ps(x1Val1, _mm_set1_ps(1/scaling));
    x2Val1 = _mm_mul_ps(x2Val1, _mm_set1_ps(1/scaling));

    _mm_store_ps(xPtr, x1Val1); xPtr+=4;
    _mm_store_ps(xPtr, x2Val1); xPtr+=4;
    
  }
  for (int i=8*(nof_symbols/8);i<nof_symbols;i++) {
    cf_t r  = 0; 
    cf_t hh = 0; 
    for (int p=0;p<nof_rxant;p++) {
      r  += y[p][i]*conj(h[p][i]);
      hh += conj(h[p][i])*h[p][i];
    }
    x[i] = scaling*r/(hh+noise_estimate);
  }
  return nof_symbols;
}

#endif

#ifdef LV_HAVE_AVX

#define PROD_AVX(a,b) _mm256_addsub_ps(_mm256_mul_ps(a,_mm256_moveldup_ps(b)),_mm256_mul_ps(_mm256_shuffle_ps(a,a,0xB1),_mm256_movehdup_ps(b)))



int srslte_predecoding_single_avx(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x, int nof_rxant, int nof_symbols, float scaling, float noise_estimate) {
  
  float *xPtr = (float*) x;
  const float *hPtr1 = (const float*) h[0];
  const float *yPtr1 = (const float*) y[0];
  const float *hPtr2 = (const float*) h[1];
  const float *yPtr2 = (const float*) y[1];

  __m256 conjugator = _mm256_setr_ps(0, -0.f, 0, -0.f, 0, -0.f, 0, -0.f);
  
  __m256 noise = _mm256_set1_ps(noise_estimate);
  __m256 h1Val1, h2Val1, y1Val1, y2Val1, h12square, h1square, h2square, h1_p, h2_p, h1conj1, h2conj1, x1Val, x2Val;
  __m256 h1Val2, h2Val2, y1Val2, y2Val2, h1conj2, h2conj2;
  __m256 avx_scaling = _mm256_set1_ps(1/scaling);


  for (int i=0;i<nof_symbols/8;i++) {
    y1Val1 = _mm256_load_ps(yPtr1); yPtr1+=8;
    y2Val1 = _mm256_load_ps(yPtr1); yPtr1+=8;
    h1Val1 = _mm256_load_ps(hPtr1); hPtr1+=8;
    h2Val1 = _mm256_load_ps(hPtr1); hPtr1+=8;

    if (nof_rxant == 2) {
      y1Val2 = _mm256_load_ps(yPtr2); yPtr2+=8;
      y2Val2 = _mm256_load_ps(yPtr2); yPtr2+=8;
      h1Val2 = _mm256_load_ps(hPtr2); hPtr2+=8;
      h2Val2 = _mm256_load_ps(hPtr2); hPtr2+=8;      
    }
    
    __m256 t1 = _mm256_mul_ps(h1Val1, h1Val1);
    __m256 t2 = _mm256_mul_ps(h2Val1, h2Val1);
    h12square = _mm256_hadd_ps(_mm256_permute2f128_ps(t1, t2, 0x20), _mm256_permute2f128_ps(t1, t2, 0x31)); 

    if (nof_rxant == 2) {
      t1 = _mm256_mul_ps(h1Val2, h1Val2);
      t2 = _mm256_mul_ps(h2Val2, h2Val2);
      h12square = _mm256_add_ps(h12square, _mm256_hadd_ps(_mm256_permute2f128_ps(t1, t2, 0x20), _mm256_permute2f128_ps(t1, t2, 0x31)));   
    }

    if (noise_estimate > 0) {
      h12square  = _mm256_add_ps(h12square, noise);
    }
    
    h1_p     = _mm256_permute_ps(h12square, _MM_SHUFFLE(1, 1, 0, 0));
    h2_p     = _mm256_permute_ps(h12square, _MM_SHUFFLE(3, 3, 2, 2));
    h1square = _mm256_permute2f128_ps(h1_p, h2_p, 2<<4);
    h2square = _mm256_permute2f128_ps(h1_p, h2_p, 3<<4 | 1);
    
    /* Conjugate channel */
    h1conj1 = _mm256_xor_ps(h1Val1, conjugator); 
    h2conj1 = _mm256_xor_ps(h2Val1, conjugator); 

    if (nof_rxant == 2) {
      h1conj2 = _mm256_xor_ps(h1Val2, conjugator); 
      h2conj2 = _mm256_xor_ps(h2Val2, conjugator);       
    }
    
    /* Complex product */      
    x1Val = PROD_AVX(y1Val1, h1conj1);
    x2Val = PROD_AVX(y2Val1, h2conj1);

    if (nof_rxant == 2) {
      x1Val = _mm256_add_ps(x1Val, PROD_AVX(y1Val2, h1conj2));
      x2Val = _mm256_add_ps(x2Val, PROD_AVX(y2Val2, h2conj2));  
    }
    
    x1Val = _mm256_div_ps(x1Val, h1square);
    x2Val = _mm256_div_ps(x2Val, h2square);

    x1Val = _mm256_mul_ps(x1Val, avx_scaling);
    x2Val = _mm256_mul_ps(x2Val, avx_scaling);

    _mm256_store_ps(xPtr, x1Val); xPtr+=8;
    _mm256_store_ps(xPtr, x2Val); xPtr+=8;
  }
  for (int i=16*(nof_symbols/16);i<nof_symbols;i++) {
    cf_t r  = 0; 
    cf_t hh = 0; 
    for (int p=0;p<nof_rxant;p++) {
      r  += y[p][i]*conj(h[p][i]);
      hh += conj(h[p][i])*h[p][i];
    }
    x[i] = r/((hh+noise_estimate) * scaling);
  }
  return nof_symbols;
}

#endif

int srslte_predecoding_single_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x, int nof_rxant, int nof_symbols, float scaling, float noise_estimate) {
  for (int i=0;i<nof_symbols;i++) {
    cf_t r  = 0; 
    cf_t hh = 0; 
    for (int p=0;p<nof_rxant;p++) {
      r  += y[p][i]*conj(h[p][i]);
      hh += conj(h[p][i])*h[p][i];
    }
    x[i] = r / ((hh+noise_estimate) * scaling);
  }
  return nof_symbols;
}

/* ZF/MMSE SISO equalizer x=y(h'h+no)^(-1)h' (ZF if n0=0.0)*/
int srslte_predecoding_single(cf_t *y_, cf_t *h_, cf_t *x, int nof_symbols, float scaling, float noise_estimate) {
  
  cf_t *y[SRSLTE_MAX_PORTS]; 
  cf_t *h[SRSLTE_MAX_PORTS];
  y[0] = y_;
  h[0] = h_; 
  int nof_rxant = 1; 
  
#ifdef LV_HAVE_AVX
  if (nof_symbols > 32 && nof_rxant <= 2) {
    return srslte_predecoding_single_avx(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  } else {
    return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  }
#else
  #ifdef LV_HAVE_SSE
    if (nof_symbols > 32 && nof_rxant <= 2) {
      return srslte_predecoding_single_sse(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
    } else {
      return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
    }
  #else
    return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  #endif
#endif
}

/* ZF/MMSE SISO equalizer x=y(h'h+no)^(-1)h' (ZF if n0=0.0)*/
int srslte_predecoding_single_multi(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS], cf_t *x,
                                    int nof_rxant, int nof_symbols, float scaling, float noise_estimate) {
#ifdef LV_HAVE_AVX
  if (nof_symbols > 32) {
    return srslte_predecoding_single_avx(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  } else {
    return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  }
#else
  #ifdef LV_HAVE_SSE
    if (nof_symbols > 32) {
      return srslte_predecoding_single_sse(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
    } else {
      return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
    }
  #else
    return srslte_predecoding_single_gen(y, h, x, nof_rxant, nof_symbols, scaling, noise_estimate);
  #endif
#endif
}

/* C implementatino of the SFBC equalizer */
int srslte_predecoding_diversity_gen_(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], 
                                      cf_t *x[SRSLTE_MAX_LAYERS], 
                                      int nof_rxant, int nof_ports, int nof_symbols, int symbol_start, float scaling)
{
  int i;
  if (nof_ports == 2) {
    cf_t h00, h01, h10, h11, r0, r1;

    for (i = symbol_start/2; i < nof_symbols / 2; i++) {
      float hh = 0;
      cf_t x0 = 0; 
      cf_t x1 = 0; 
      for (int p=0;p<nof_rxant;p++) {
        h00 = h[0][p][2 * i];
        h01 = h[0][p][2 * i+1];
        h10 = h[1][p][2 * i];
        h11 = h[1][p][2 * i+1];
        hh += crealf(h00) * crealf(h00) + cimagf(h00) * cimagf(h00)
            + crealf(h11) * crealf(h11) + cimagf(h11) * cimagf(h11);
        r0 = y[p][2 * i];
        r1 = y[p][2 * i + 1];
        if (hh == 0) {
          hh = 1e-4;
        }
        x0 += (conjf(h00) * r0 + h11 * conjf(r1));
        x1 += (-h10 * conj(r0) + conj(h01) * r1);
      }
      hh *= scaling;
      x[0][i] = x0 / hh * sqrt(2);
      x[1][i] = x1 / hh * sqrt(2);
    }
    return i;
  } else if (nof_ports == 4) {
    cf_t h0, h1, h2, h3, r0, r1, r2, r3;
    
    int m_ap = (nof_symbols % 4) ? ((nof_symbols - 2) / 4) : nof_symbols / 4;
    for (i = symbol_start; i < m_ap; i++) {
      float hh02 = 0, hh13 = 0;
      cf_t x0 = 0, x1 = 0, x2 = 0, x3 = 0; 
      for (int p=0;p<nof_rxant;p++) {
        h0 = h[0][p][4 * i];
        h1 = h[1][p][4 * i + 2];
        h2 = h[2][p][4 * i];
        h3 = h[3][p][4 * i + 2];
        hh02 += crealf(h0) * crealf(h0) + cimagf(h0) * cimagf(h0)
            + crealf(h2) * crealf(h2) + cimagf(h2) * cimagf(h2);
        hh13 += crealf(h1) * crealf(h1) + cimagf(h1) * cimagf(h1)
            + crealf(h3) * crealf(h3) + cimagf(h3) * cimagf(h3);
        r0 = y[p][4 * i];
        r1 = y[p][4 * i + 1];
        r2 = y[p][4 * i + 2];
        r3 = y[p][4 * i + 3];

        x0 += (conjf(h0) * r0 + h2 * conjf(r1));
        x1 += (-h2 * conjf(r0) + conjf(h0) * r1);
        x2 += (conjf(h1) * r2 + h3 * conjf(r3));
        x3 += (-h3 * conjf(r2) + conjf(h1) * r3);
      }

      hh02 *= scaling;
      hh13 *= scaling;

      x[0][i] = x0 / hh02 * sqrt(2);
      x[1][i] = x1 / hh02 * sqrt(2);
      x[2][i] = x2 / hh13 * sqrt(2);
      x[3][i] = x3 / hh13 * sqrt(2);
    }
    return i;
  } else {
    fprintf(stderr, "Number of ports must be 2 or 4 for transmit diversity (nof_ports=%d)\n", nof_ports);
    return -1;
  }
}

int srslte_predecoding_diversity_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], 
                                     cf_t *x[SRSLTE_MAX_LAYERS], 
                                     int nof_rxant, int nof_ports, int nof_symbols, float scaling) {
  return srslte_predecoding_diversity_gen_(y, h, x, nof_rxant, nof_ports, nof_symbols, 0, scaling);
}

/* SSE implementation of the 2-port SFBC equalizer */
#ifdef LV_HAVE_SSE
int srslte_predecoding_diversity2_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], 
                                      cf_t *x[SRSLTE_MAX_LAYERS], 
                                      int nof_rxant, int nof_symbols, float scaling)
{
  float *x0Ptr = (float*) x[0];
  float *x1Ptr = (float*) x[1];
  const float *h0Ptr0 = (const float*) h[0][0];
  const float *h1Ptr0 = (const float*) h[1][0];
  const float *h0Ptr1 = (const float*) h[0][1];
  const float *h1Ptr1 = (const float*) h[1][1];
  const float *yPtr0 = (const float*) y[0];
  const float *yPtr1 = (const float*) y[1];

  __m128 conjugator = _mm_setr_ps(0, -0.f, 0, -0.f);
  __m128 sqrt2      = _mm_set1_ps(sqrtf(2)/scaling);
  
  __m128 h0Val_00, h0Val_10, h1Val_00, h1Val_10, h000, h00conj0, h010, h01conj0, h100, h110;
  __m128 h0Val_01, h0Val_11, h1Val_01, h1Val_11, h001, h00conj1, h011, h01conj1, h101, h111;
  __m128 hh, hhshuf, hhsum, hhadd; 
  __m128 r0Val0, r1Val0, r00, r10, r0conj0, r1conj0; 
  __m128 r0Val1, r1Val1, r01, r11, r0conj1, r1conj1; 
  __m128 x0, x1; 
  
  for (int i=0;i<nof_symbols/4;i++) {
  
    h0Val_00 = _mm_load_ps(h0Ptr0); h0Ptr0+=4; h0Val_10 = _mm_load_ps(h0Ptr0); h0Ptr0+=4;    
    h1Val_00 = _mm_load_ps(h1Ptr0); h1Ptr0+=4; h1Val_10 = _mm_load_ps(h1Ptr0); h1Ptr0+=4;

    if (nof_rxant == 2) {
      h0Val_01 = _mm_load_ps(h0Ptr1); h0Ptr1+=4; h0Val_11 = _mm_load_ps(h0Ptr1); h0Ptr1+=4;    
      h1Val_01 = _mm_load_ps(h1Ptr1); h1Ptr1+=4; h1Val_11 = _mm_load_ps(h1Ptr1); h1Ptr1+=4;
    }
    
    h000 = _mm_shuffle_ps(h0Val_00, h0Val_10, _MM_SHUFFLE(1, 0, 1, 0));
    h010 = _mm_shuffle_ps(h0Val_00, h0Val_10, _MM_SHUFFLE(3, 2, 3, 2));
    
    h100 = _mm_shuffle_ps(h1Val_00, h1Val_10, _MM_SHUFFLE(1, 0, 1, 0));
    h110 = _mm_shuffle_ps(h1Val_00, h1Val_10, _MM_SHUFFLE(3, 2, 3, 2));

    if (nof_rxant == 2) {
      h001 = _mm_shuffle_ps(h0Val_01, h0Val_11, _MM_SHUFFLE(1, 0, 1, 0));
      h011 = _mm_shuffle_ps(h0Val_01, h0Val_11, _MM_SHUFFLE(3, 2, 3, 2));
      
      h101 = _mm_shuffle_ps(h1Val_01, h1Val_11, _MM_SHUFFLE(1, 0, 1, 0));
      h111 = _mm_shuffle_ps(h1Val_01, h1Val_11, _MM_SHUFFLE(3, 2, 3, 2));      
    }
    
    r0Val0 = _mm_load_ps(yPtr0); yPtr0+=4;
    r1Val0 = _mm_load_ps(yPtr0); yPtr0+=4;
    r00 = _mm_shuffle_ps(r0Val0, r1Val0, _MM_SHUFFLE(1, 0, 1, 0));
    r10 = _mm_shuffle_ps(r0Val0, r1Val0, _MM_SHUFFLE(3, 2, 3, 2));

    if (nof_rxant == 2) {
      r0Val1 = _mm_load_ps(yPtr1); yPtr1+=4;
      r1Val1 = _mm_load_ps(yPtr1); yPtr1+=4;
      r01 = _mm_shuffle_ps(r0Val1, r1Val1, _MM_SHUFFLE(1, 0, 1, 0));
      r11 = _mm_shuffle_ps(r0Val1, r1Val1, _MM_SHUFFLE(3, 2, 3, 2));      
    }
    
    /* Compute channel gain */
    hhadd  = _mm_hadd_ps(_mm_mul_ps(h000, h000), _mm_mul_ps(h110, h110)); 
    hhshuf = _mm_shuffle_ps(hhadd, hhadd, _MM_SHUFFLE(3, 1, 2, 0));
    hhsum  = _mm_hadd_ps(hhshuf, hhshuf);
    hh     = _mm_shuffle_ps(hhsum, hhsum, _MM_SHUFFLE(1, 1, 0, 0)); // h00^2+h11^2 
    
    /* Add channel from 2nd antenna */
    if (nof_rxant == 2) {
      hhadd  = _mm_hadd_ps(_mm_mul_ps(h001, h001), _mm_mul_ps(h111, h111)); 
      hhshuf = _mm_shuffle_ps(hhadd, hhadd, _MM_SHUFFLE(3, 1, 2, 0));
      hhsum  = _mm_hadd_ps(hhshuf, hhshuf);
      hh     = _mm_add_ps(hh, _mm_shuffle_ps(hhsum, hhsum, _MM_SHUFFLE(1, 1, 0, 0))); // h00^2+h11^2       
    }
    
    // Conjugate value 
    h00conj0 = _mm_xor_ps(h000, conjugator);
    h01conj0 = _mm_xor_ps(h010, conjugator); 
    r0conj0  = _mm_xor_ps(r00, conjugator);
    r1conj0  = _mm_xor_ps(r10, conjugator); 
 
    if (nof_rxant == 2) {
      h00conj1 = _mm_xor_ps(h001, conjugator);
      h01conj1 = _mm_xor_ps(h011, conjugator); 
      r0conj1  = _mm_xor_ps(r01, conjugator);
      r1conj1  = _mm_xor_ps(r11, conjugator);       
    }
    
    // Multiply by channel matrix
    x0 = _mm_add_ps(PROD(h00conj0, r00), PROD(h110, r1conj0));
    x1 = _mm_sub_ps(PROD(h01conj0, r10), PROD(h100, r0conj0));

    // Add received symbol from 2nd antenna
    if (nof_rxant == 2) {
      x0 = _mm_add_ps(x0, _mm_add_ps(PROD(h00conj1, r01), PROD(h111, r1conj1)));
      x1 = _mm_add_ps(x1, _mm_sub_ps(PROD(h01conj1, r11), PROD(h101, r0conj1)));                
    }

    x0 = _mm_mul_ps(_mm_div_ps(x0, hh), sqrt2);
    x1 = _mm_mul_ps(_mm_div_ps(x1, hh), sqrt2);

    _mm_store_ps(x0Ptr, x0); x0Ptr+=4;
    _mm_store_ps(x1Ptr, x1); x1Ptr+=4;    
  }
  // Compute remaining symbols using generic implementation
  srslte_predecoding_diversity_gen_(y, h, x, nof_rxant, 2, nof_symbols, 4*(nof_symbols/4), scaling);
  return nof_symbols;
}
#endif

int srslte_predecoding_diversity(cf_t *y_, cf_t *h_[SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS], 
                          int nof_ports, int nof_symbols, float scaling)
{
  cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS]; 
  cf_t *y[SRSLTE_MAX_PORTS]; 
  uint32_t nof_rxant = 1; 
  
  for (int i=0;i<nof_ports;i++) {
    h[i][0] = h_[i];
  }
  y[0] = y_; 
  
#ifdef LV_HAVE_SSE
  if (nof_symbols > 32 && nof_ports == 2) {
    return srslte_predecoding_diversity2_sse(y, h, x, nof_rxant, nof_symbols, scaling);
  } else {
    return srslte_predecoding_diversity_gen(y, h, x, nof_rxant, nof_ports, nof_symbols, scaling);
  }
#else
  return srslte_predecoding_diversity_gen(y, h, x, nof_rxant, nof_ports, nof_symbols, scaling);
#endif   
}

int srslte_predecoding_diversity_multi(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS], 
                          int nof_rxant, int nof_ports, int nof_symbols, float scaling)
{
#ifdef LV_HAVE_SSE
  if (nof_symbols > 32 && nof_ports == 2) {
    return srslte_predecoding_diversity2_sse(y, h, x, nof_rxant, nof_symbols, scaling);
  } else {
    return srslte_predecoding_diversity_gen(y, h, x, nof_rxant, nof_ports, nof_symbols, scaling);
  }
#else
  return srslte_predecoding_diversity_gen(y, h, x, nof_rxant, nof_ports, nof_symbols, scaling);
#endif   
}

int srslte_precoding_mimo_2x2_gen(cf_t W[2][2], cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS], 
                              int nof_symbols, float scaling, float noise_estimate)
{
 
  cf_t G[2][2], Gx[2][2]; 
  
  for (int i=0; i<nof_symbols; i++) {
  
    // G=H*W
    G[0][0] = h[0][0][i]*W[0][0]+h[0][1][i]*W[1][0];        
    G[0][1] = h[0][0][i]*W[1][0]+h[0][1][i]*W[1][1]; 
    G[1][0] = h[1][0][i]*W[0][0]+h[1][1][i]*W[1][0];
    G[1][1] = h[1][0][i]*W[1][0]+h[1][1][i]*W[1][1]; 
    
    if (noise_estimate == 0) {
      // MF equalizer: Gx = G'
      Gx[0][0] = conjf(G[0][0]); 
      Gx[0][1] = conjf(G[1][0]); 
      Gx[1][0] = conjf(G[0][1]); 
      Gx[1][1] = conjf(G[1][1]); 
    } else {
      // MMSE equalizer: Gx = (G'G+I)      
      fprintf(stderr, "MMSE MIMO decoder not implemented\n");
      return -1; 
    }
    
    // x=G*y
    x[0][i] = (Gx[0][0]*y[0][i] + Gx[0][1]*y[1][i]) * scaling;
    x[1][i] = (Gx[1][0]*y[0][i] + Gx[1][1]*y[1][i]) * scaling;
  }
  
  return SRSLTE_SUCCESS; 
}

// AVX implementation of ZF 2x2 CCD equalizer
#ifdef LV_HAVE_AVX

int srslte_predecoding_ccd_2x2_zf_avx(cf_t *y[SRSLTE_MAX_PORTS],
                                      cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                      cf_t *x[SRSLTE_MAX_LAYERS],
                                      uint32_t nof_symbols,
                                      float scaling) {
  uint32_t i = 0;

  __m256 mask0 = _mm256_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f);
  __m256 mask1 = _mm256_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f);

  for (i = 0; i < nof_symbols - 3; i += 4) {
    /* Load channel */
    __m256 h00i = _mm256_load_ps((float *) &h[0][0][i]);
    __m256 h01i = _mm256_load_ps((float *) &h[0][1][i]);
    __m256 h10i = _mm256_load_ps((float *) &h[1][0][i]);
    __m256 h11i = _mm256_load_ps((float *) &h[1][1][i]);

    /* Apply precoding */
    __m256 h00 = _mm256_add_ps(h00i, _mm256_xor_ps(h10i, mask0));
    __m256 h10 = _mm256_add_ps(h01i, _mm256_xor_ps(h11i, mask0));
    __m256 h01 = _mm256_add_ps(h00i, _mm256_xor_ps(h10i, mask1));
    __m256 h11 = _mm256_add_ps(h01i, _mm256_xor_ps(h11i, mask1));

    __m256 y0 = _mm256_load_ps((float *) &y[0][i]);
    __m256 y1 = _mm256_load_ps((float *) &y[1][i]);

    __m256 x0, x1;

    srslte_mat_2x2_zf_avx(y0, y1, h00, h01, h10, h11, &x0, &x1, 2.0f / scaling);

    _mm256_store_ps((float *) &x[0][i], x0);
    _mm256_store_ps((float *) &x[1][i], x1);
  }

  return nof_symbols;
}
#endif /* LV_HAVE_AVX */

// SSE implementation of ZF 2x2 CCD equalizer
#ifdef LV_HAVE_SSE

int srslte_predecoding_ccd_2x2_zf_sse(cf_t *y[SRSLTE_MAX_PORTS],
                                      cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                      cf_t *x[SRSLTE_MAX_LAYERS],
                                      uint32_t nof_symbols,
                                      float scaling) {
  uint32_t i = 0;

  for (i = 0; i < nof_symbols - 1; i += 2) {
    /* Load channel */
    __m128 h00i = _mm_load_ps((float *) &h[0][0][i]);
    __m128 h01i = _mm_load_ps((float *) &h[0][1][i]);
    __m128 h10i = _mm_load_ps((float *) &h[1][0][i]);
    __m128 h11i = _mm_load_ps((float *) &h[1][1][i]);

    /* Apply precoding */
    __m128 h00 = _mm_add_ps(h00i, _mm_xor_ps(h10i, _mm_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f)));
    __m128 h10 = _mm_add_ps(h01i, _mm_xor_ps(h11i, _mm_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f)));
    __m128 h01 = _mm_add_ps(h00i, _mm_xor_ps(h10i, _mm_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f)));
    __m128 h11 = _mm_add_ps(h01i, _mm_xor_ps(h11i, _mm_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f)));

    __m128 y0 = _mm_load_ps((float *) &y[0][i]);
    __m128 y1 = _mm_load_ps((float *) &y[1][i]);

    __m128 x0, x1;

    srslte_mat_2x2_zf_sse(y0, y1, h00, h01, h10, h11, &x0, &x1, 2.0f / scaling);

    _mm_store_ps((float *) &x[0][i], x0);
    _mm_store_ps((float *) &x[1][i], x1);
  }

  return nof_symbols;
}
#endif

// Generic implementation of ZF 2x2 CCD equalizer
int srslte_predecoding_ccd_2x2_zf_gen(cf_t *y[SRSLTE_MAX_PORTS],
                                      cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                      cf_t *x[SRSLTE_MAX_LAYERS],
                                      int nof_symbols,
                                      float scaling) {
  cf_t h00, h01, h10, h11;

  for (int i = 0; i < nof_symbols; i++) {

    // Even precoder
    h00 = +h[0][0][i] + h[1][0][i];
    h10 = +h[0][1][i] + h[1][1][i];
    h01 = +h[0][0][i] - h[1][0][i];
    h11 = +h[0][1][i] - h[1][1][i];

    srslte_mat_2x2_zf_gen(y[0][i], y[1][i], h00, h01, h10, h11, &x[0][i], &x[1][i], 2.0f / scaling);

    i++;

    // Odd precoder
    h00 = h[0][0][i] - h[1][0][i];
    h10 = h[0][1][i] - h[1][1][i];
    h01 = h[0][0][i] + h[1][0][i];
    h11 = h[0][1][i] + h[1][1][i];

    srslte_mat_2x2_zf_gen(y[0][i], y[1][i], h00, h01, h10, h11, &x[0][i], &x[1][i], 2.0f / scaling);
  }
  return SRSLTE_SUCCESS;
}

int srslte_predecoding_ccd_zf(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS],
                              int nof_rxant, int nof_ports, int nof_layers, int nof_symbols, float scaling)
{
  if (nof_ports == 2 && nof_rxant == 2) {
    if (nof_layers == 2) {
#ifdef LV_HAVE_AVX
      return srslte_predecoding_ccd_2x2_zf_avx(y, h, x, nof_symbols, scaling);
#else
#ifdef LV_HAVE_SSE
      return srslte_predecoding_ccd_2x2_zf_sse(y, h, x, nof_symbols, scaling);
#else
      return srslte_predecoding_ccd_2x2_zf_gen(y, h, x, nof_symbols, scaling);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
    } else {
      DEBUG("Error predecoding CCD: Invalid number of layers %d\n", nof_layers);
      return -1;       
    }          
  } else if (nof_ports == 4) {
    DEBUG("Error predecoding CCD: Only 2 ports supported\n");
  } else {
    DEBUG("Error predecoding CCD: Invalid combination of ports %d and rx antennax %d\n", nof_ports, nof_rxant);
  }
  return SRSLTE_ERROR;
}

// AVX implementation of MMSE 2x2 CCD equalizer
#ifdef LV_HAVE_AVX

int srslte_predecoding_ccd_2x2_mmse_avx(cf_t *y[SRSLTE_MAX_PORTS],
                                        cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS],
                                        uint32_t nof_symbols, float scaling, float noise_estimate) {
  uint32_t i = 0;

  for (i = 0; i < nof_symbols - 3; i += 4) {
    /* Load channel */
    __m256 h00i = _mm256_load_ps((float *) &h[0][0][i]);
    __m256 h01i = _mm256_load_ps((float *) &h[0][1][i]);
    __m256 h10i = _mm256_load_ps((float *) &h[1][0][i]);
    __m256 h11i = _mm256_load_ps((float *) &h[1][1][i]);

    /* Apply precoding */
    __m256 h00 = _mm256_add_ps(h00i, _mm256_xor_ps(h10i, _mm256_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f)));
    __m256 h10 = _mm256_add_ps(h01i, _mm256_xor_ps(h11i, _mm256_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f)));
    __m256 h01 = _mm256_add_ps(h00i, _mm256_xor_ps(h10i, _mm256_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f)));
    __m256 h11 = _mm256_add_ps(h01i, _mm256_xor_ps(h11i, _mm256_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f)));

    __m256 y0 = _mm256_load_ps((float *) &y[0][i]);
    __m256 y1 = _mm256_load_ps((float *) &y[1][i]);

    __m256 x0, x1;

    srslte_mat_2x2_mmse_avx(y0, y1, h00, h01, h10, h11, &x0, &x1, noise_estimate, 2.0f / scaling);

    _mm256_store_ps((float *) &x[0][i], x0);
    _mm256_store_ps((float *) &x[1][i], x1);
  }

  return nof_symbols;
}
#endif /* LV_HAVE_AVX */

// SSE implementation of ZF 2x2 CCD equalizer
#ifdef LV_HAVE_SSE

int srslte_predecoding_ccd_2x2_mmse_sse(cf_t *y[SRSLTE_MAX_PORTS],
                                      cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                      cf_t *x[SRSLTE_MAX_LAYERS],
                                      uint32_t nof_symbols, float scaling, float noise_estimate) {
  uint32_t i = 0;

  for (i = 0; i < nof_symbols - 1; i += 2) {
    /* Load channel */
    __m128 h00i = _mm_load_ps((float *) &h[0][0][i]);
    __m128 h01i = _mm_load_ps((float *) &h[0][1][i]);
    __m128 h10i = _mm_load_ps((float *) &h[1][0][i]);
    __m128 h11i = _mm_load_ps((float *) &h[1][1][i]);

    /* Apply precoding */
    __m128 h00 = _mm_add_ps(h00i, _mm_xor_ps(h10i, _mm_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f)));
    __m128 h10 = _mm_add_ps(h01i, _mm_xor_ps(h11i, _mm_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f)));
    __m128 h01 = _mm_add_ps(h00i, _mm_xor_ps(h10i, _mm_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f)));
    __m128 h11 = _mm_add_ps(h01i, _mm_xor_ps(h11i, _mm_setr_ps(-0.0f, -0.0f, +0.0f, +0.0f)));

    __m128 y0 = _mm_load_ps((float *) &y[0][i]);
    __m128 y1 = _mm_load_ps((float *) &y[1][i]);

    __m128 x0, x1;

    srslte_mat_2x2_mmse_sse(y0, y1, h00, h01, h10, h11, &x0, &x1, noise_estimate, 2.0f / scaling);

    _mm_store_ps((float *) &x[0][i], x0);
    _mm_store_ps((float *) &x[1][i], x1);
  }

  return nof_symbols;
}
#endif /* LV_HAVE_SSE */

// Generic implementation of ZF 2x2 CCD equalizer
int srslte_predecoding_ccd_2x2_mmse_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS],
                                      int nof_symbols, float scaling, float noise_estimate) {
  cf_t h00, h01, h10, h11;

  for (int i = 0; i < nof_symbols; i++) {

    // Even precoder
    h00 = +h[0][0][i] + h[1][0][i];
    h10 = +h[0][1][i] + h[1][1][i];
    h01 = +h[0][0][i] - h[1][0][i];
    h11 = +h[0][1][i] - h[1][1][i];
    srslte_mat_2x2_mmse_gen(y[0][i], y[1][i], h00, h01, h10, h11, &x[0][i], &x[1][i], noise_estimate, 2.0f / scaling);

    i++;

    // Odd precoder
    h00 = h[0][0][i] - h[1][0][i];
    h10 = h[0][1][i] - h[1][1][i];
    h01 = h[0][0][i] + h[1][0][i];
    h11 = h[0][1][i] + h[1][1][i];
    srslte_mat_2x2_mmse_gen(y[0][i], y[1][i], h00, h01, h10, h11, &x[0][i], &x[1][i], noise_estimate, 2.0f / scaling);
  }
  return SRSLTE_SUCCESS;
}


int srslte_predecoding_ccd_mmse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS],
                              int nof_rxant, int nof_ports, int nof_layers, int nof_symbols, float scaling, float noise_estimate)
{
  if (nof_ports == 2 && nof_rxant == 2) {
    if (nof_layers == 2) {
#ifdef LV_HAVE_AVX
      return srslte_predecoding_ccd_2x2_mmse_avx(y, h, x, nof_symbols, scaling, noise_estimate);
#else
#ifdef LV_HAVE_SSE
      return srslte_predecoding_ccd_2x2_mmse_sse(y, h, x, nof_symbols, scaling, noise_estimate);
#else
      return srslte_predecoding_ccd_2x2_mmse_gen(y, h, x, nof_symbols, scaling, noise_estimate);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
    } else {
      DEBUG("Error predecoding CCD: Invalid number of layers %d\n", nof_layers);
      return -1;
    }
  } else if (nof_ports == 4) {
    DEBUG("Error predecoding CCD: Only 2 ports supported\n");
  } else {
    DEBUG("Error predecoding CCD: Invalid combination of ports %d and rx antennax %d\n", nof_ports, nof_rxant);
  }
  return SRSLTE_ERROR;
}

#ifdef LV_HAVE_AVX

// Generic implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_zf_avx(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {
  float norm = 1.0;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      DEBUG("Wrong codebook_idx=%d", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols - 3; i += 4) {
    __m256 _h00 = _mm256_load_ps((float*)&(h[0][0][i]));
    __m256 _h01 = _mm256_load_ps((float*)&(h[0][1][i]));
    __m256 _h10 = _mm256_load_ps((float*)&(h[1][0][i]));
    __m256 _h11 = _mm256_load_ps((float*)&(h[1][1][i]));

    __m256 h00, h01, h10, h11;
    switch (codebook_idx) {
      case 0:
        h00 = _h00;
        h01 = _h10;
        h10 = _h01;
        h11 = _h11;
        break;
      case 1:
        h00 = _mm256_add_ps(_h00, _h10);
        h01 = _mm256_sub_ps(_h00, _h10);
        h10 = _mm256_add_ps(_h01, _h11);
        h11 = _mm256_sub_ps(_h01, _h11);
        break;
      case 2:
        h00 = _mm256_add_ps(_h00, _MM256_MULJ_PS(_h10));
        h01 = _mm256_sub_ps(_h00, _MM256_MULJ_PS(_h10));
        h10 = _mm256_add_ps(_h01, _MM256_MULJ_PS(_h11));
        h11 = _mm256_sub_ps(_h01, _MM256_MULJ_PS(_h11));
        break;
      default:
        DEBUG("Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m256 y0 = _mm256_load_ps((float *) &y[0][i]);
    __m256 y1 = _mm256_load_ps((float *) &y[1][i]);

    __m256 x0, x1;

    srslte_mat_2x2_zf_avx(y0, y1, h00, h01, h10, h11, &x0, &x1, norm);

    _mm256_store_ps((float *) &x[0][i], x0);
    _mm256_store_ps((float *) &x[1][i], x1);

  }

  return SRSLTE_SUCCESS;
}

#endif /* LV_HAVE_AVX */

#ifdef LV_HAVE_SSE

// SSE implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_zf_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {
  float norm = 1.0;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      ERROR("Wrong codebook_idx=%d", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols - 1; i += 2) {
    __m128 _h00 = _mm_load_ps((float*)&(h[0][0][i]));
    __m128 _h01 = _mm_load_ps((float*)&(h[0][1][i]));
    __m128 _h10 = _mm_load_ps((float*)&(h[1][0][i]));
    __m128 _h11 = _mm_load_ps((float*)&(h[1][1][i]));

    __m128 h00, h01, h10, h11;
    switch (codebook_idx) {
      case 0:
        h00 = _h00;
        h01 = _h10;
        h10 = _h01;
        h11 = _h11;
        break;
      case 1:
        h00 = _mm_add_ps(_h00, _h10);
        h01 = _mm_sub_ps(_h00, _h10);
        h10 = _mm_add_ps(_h01, _h11);
        h11 = _mm_sub_ps(_h01, _h11);
        break;
      case 2:
        h00 = _mm_add_ps(_h00, _MM_MULJ_PS(_h10));
        h01 = _mm_sub_ps(_h00, _MM_MULJ_PS(_h10));
        h10 = _mm_add_ps(_h01, _MM_MULJ_PS(_h11));
        h11 = _mm_sub_ps(_h01, _MM_MULJ_PS(_h11));
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m128 y0 = _mm_load_ps((float *) &y[0][i]);
    __m128 y1 = _mm_load_ps((float *) &y[1][i]);

    __m128 x0, x1;

    srslte_mat_2x2_zf_sse(y0, y1, h00, h01, h10, h11, &x0, &x1, norm);

    _mm_store_ps((float *) &x[0][i], x0);
    _mm_store_ps((float *) &x[1][i], x1);

  }

  return SRSLTE_SUCCESS;
}

#endif /* LV_HAVE_SSE */


// Generic implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_zf_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {
  float norm = 1.0;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols; i++) {
    cf_t h00, h01, h10, h11, det;

    switch(codebook_idx) {
      case 0:
        h00 = h[0][0][i];
        h01 = h[1][0][i];
        h10 = h[0][1][i];
        h11 = h[1][1][i];
        break;
      case 1:
        h00 = h[0][0][i] + h[1][0][i];
        h01 = h[0][0][i] - h[1][0][i];
        h10 = h[0][1][i] + h[1][1][i];
        h11 = h[0][1][i] - h[1][1][i];
        break;
      case 2:
        h00 = h[0][0][i] + _Complex_I*h[1][0][i];
        h01 = h[0][0][i] - _Complex_I*h[1][0][i];
        h10 = h[0][1][i] + _Complex_I*h[1][1][i];
        h11 = h[0][1][i] - _Complex_I*h[1][1][i];
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    det = (h00 * h11 - h01 * h10);
    det = conjf(det) * (norm / (crealf(det) * crealf(det) + cimagf(det) * cimagf(det)));

    x[0][i] = (+h11 * y[0][i] - h01 * y[1][i]) * det;
    x[1][i] = (-h10 * y[0][i] + h00 * y[1][i]) * det;
  }
  return SRSLTE_SUCCESS;
}

#ifdef LV_HAVE_AVX

// AVX implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_mmse_avx(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols,
                                        float scaling, float noise_estimate) {
  float norm = 1.0;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols; i += 4) {
    __m256 _h00 = _mm256_load_ps((float*)&(h[0][0][i]));
    __m256 _h01 = _mm256_load_ps((float*)&(h[0][1][i]));
    __m256 _h10 = _mm256_load_ps((float*)&(h[1][0][i]));
    __m256 _h11 = _mm256_load_ps((float*)&(h[1][1][i]));

    __m256 h00, h01, h10, h11;
    switch (codebook_idx) {
      case 0:
        h00 = _h00;
        h01 = _h10;
        h10 = _h01;
        h11 = _h11;
        break;
      case 1:
        h00 = _mm256_add_ps(_h00, _h10);
        h01 = _mm256_sub_ps(_h00, _h10);
        h10 = _mm256_add_ps(_h01, _h11);
        h11 = _mm256_sub_ps(_h01, _h11);
        break;
      case 2:
        h00 = _mm256_add_ps(_h00, _MM256_MULJ_PS(_h10));
        h01 = _mm256_sub_ps(_h00, _MM256_MULJ_PS(_h10));
        h10 = _mm256_add_ps(_h01, _MM256_MULJ_PS(_h11));
        h11 = _mm256_sub_ps(_h01, _MM256_MULJ_PS(_h11));
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m256 y0 = _mm256_load_ps((float *) &y[0][i]);
    __m256 y1 = _mm256_load_ps((float *) &y[1][i]);

    __m256 x0, x1;

    srslte_mat_2x2_mmse_avx(y0, y1, h00, h01, h10, h11, &x0, &x1, noise_estimate, norm);

    _mm256_store_ps((float *) &x[0][i], x0);
    _mm256_store_ps((float *) &x[1][i], x1);

  }

  return SRSLTE_SUCCESS;
}

#endif /* LV_HAVE_AVX */


#ifdef LV_HAVE_SSE

// SSE implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_mmse_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols,
                                        float scaling, float noise_estimate) {
  float norm;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols - 1; i += 2) {
    __m128 _h00 = _mm_load_ps((float*)&(h[0][0][i]));
    __m128 _h01 = _mm_load_ps((float*)&(h[0][1][i]));
    __m128 _h10 = _mm_load_ps((float*)&(h[1][0][i]));
    __m128 _h11 = _mm_load_ps((float*)&(h[1][1][i]));

    __m128 h00, h01, h10, h11;
    switch (codebook_idx) {
      case 0:
        h00 = _h00;
        h01 = _h10;
        h10 = _h01;
        h11 = _h11;
        break;
      case 1:
        h00 = _mm_add_ps(_h00, _h10);
        h01 = _mm_sub_ps(_h00, _h10);
        h10 = _mm_add_ps(_h01, _h11);
        h11 = _mm_sub_ps(_h01, _h11);
        break;
      case 2:
        h00 = _mm_add_ps(_h00, _MM_MULJ_PS(_h10));
        h01 = _mm_sub_ps(_h00, _MM_MULJ_PS(_h10));
        h10 = _mm_add_ps(_h01, _MM_MULJ_PS(_h11));
        h11 = _mm_sub_ps(_h01, _MM_MULJ_PS(_h11));
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m128 y0 = _mm_load_ps((float *) &y[0][i]);
    __m128 y1 = _mm_load_ps((float *) &y[1][i]);

    __m128 x0, x1;

    srslte_mat_2x2_mmse_sse(y0, y1, h00, h01, h10, h11, &x0, &x1, noise_estimate, norm);

    _mm_store_ps((float *) &x[0][i], x0);
    _mm_store_ps((float *) &x[1][i], x1);

  }

  return SRSLTE_SUCCESS;
}
#endif /* LV_HAVE_SSE */

// Generic implementation of ZF 2x2 Spatial Multiplexity equalizer
int srslte_predecoding_multiplex_2x2_mmse_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols,
                                        float scaling, float noise_estimate) {
  float norm = 1.0;

  switch(codebook_idx) {
    case 0:
      norm = (float) M_SQRT2 / scaling;
      break;
    case 1:
    case 2:
      norm = 2.0f / scaling;
      break;
    default:
      ERROR("Wrong codebook_idx=%d", codebook_idx);
      return SRSLTE_ERROR;
  }

  for (int i = 0; i < nof_symbols; i++) {
    cf_t h00, h01, h10, h11;

    switch(codebook_idx) {
      case 0:
        h00 = h[0][0][i];
        h01 = h[1][0][i];
        h10 = h[0][1][i];
        h11 = h[1][1][i];
        break;
      case 1:
        h00 = h[0][0][i] + h[1][0][i];
        h01 = h[0][0][i] - h[1][0][i];
        h10 = h[0][1][i] + h[1][1][i];
        h11 = h[0][1][i] - h[1][1][i];
        break;
      case 2:
        h00 = h[0][0][i] + _Complex_I*h[1][0][i];
        h01 = h[0][0][i] - _Complex_I*h[1][0][i];
        h10 = h[0][1][i] + _Complex_I*h[1][1][i];
        h11 = h[0][1][i] - _Complex_I*h[1][1][i];
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    srslte_mat_2x2_mmse_gen(y[0][i], y[1][i], h00, h01, h10, h11, &x[0][i], &x[1][i], noise_estimate, norm);
  }
  return SRSLTE_SUCCESS;
}

#ifdef LV_HAVE_AVX

// Generic implementation of MRC 2x1 (two antennas into one layer) Spatial Multiplexing equalizer
int srslte_predecoding_multiplex_2x1_mrc_avx(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                             cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {

  for (int i = 0; i < nof_symbols - 3; i += 4) {
    __m256 _h00 = _mm256_load_ps((float*)&(h[0][0][i]));
    __m256 _h01 = _mm256_load_ps((float*)&(h[0][1][i]));
    __m256 _h10 = _mm256_load_ps((float*)&(h[1][0][i]));
    __m256 _h11 = _mm256_load_ps((float*)&(h[1][1][i]));

    __m256 h0, h1;
    switch (codebook_idx) {
      case 0:
        h0 = _mm256_add_ps(_h00, _h10);
        h1 = _mm256_add_ps(_h01, _h11);
        break;
      case 1:
        h0 = _mm256_sub_ps(_h00, _h10);
        h1 = _mm256_sub_ps(_h01, _h11);
        break;
      case 2:
        h0 = _mm256_add_ps(_h00, _MM256_MULJ_PS(_h10));
        h1 = _mm256_add_ps(_h01, _MM256_MULJ_PS(_h11));
        break;
      case 3:
        h0 = _mm256_sub_ps(_h00, _MM256_MULJ_PS(_h10));
        h1 = _mm256_sub_ps(_h01, _MM256_MULJ_PS(_h11));
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m256 h0_2 = _mm256_mul_ps(h0, h0);
    __m256 h1_2 = _mm256_mul_ps(h1, h1);
    __m256 hh0 = _mm256_add_ps(_mm256_movehdup_ps(h0_2), _mm256_moveldup_ps(h0_2));
    __m256 hh1 = _mm256_add_ps(_mm256_movehdup_ps(h1_2), _mm256_moveldup_ps(h1_2));
    __m256 hh = _mm256_add_ps(hh0, hh1);
    __m256 hhrec = _mm256_rcp_ps(hh);

    hhrec = _mm256_mul_ps(hhrec, _mm256_set1_ps((float) M_SQRT2 / scaling));
    __m256 y0 = _mm256_load_ps((float*)&y[0][i]);
    __m256 y1 = _mm256_load_ps((float*)&y[1][i]);

    __m256 x0 = _mm256_add_ps(_MM256_PROD_PS(_MM256_CONJ_PS(h0), y0), _MM256_PROD_PS(_MM256_CONJ_PS(h1), y1));
    x0 = _mm256_mul_ps(hhrec, x0);

    _mm256_store_ps((float*)&x[0][i], x0);

  }

  return SRSLTE_SUCCESS;
}

#endif /* LV_HAVE_AVX */


// SSE  implementation of MRC 2x1 (two antennas into one layer) Spatial Multiplexing equalizer
#ifdef LV_HAVE_SSE

int srslte_predecoding_multiplex_2x1_mrc_sse(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                             cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {

  for (int i = 0; i < nof_symbols - 1; i += 2) {
    __m128 _h00 = _mm_load_ps((float*)&(h[0][0][i]));
    __m128 _h01 = _mm_load_ps((float*)&(h[0][1][i]));
    __m128 _h10 = _mm_load_ps((float*)&(h[1][0][i]));
    __m128 _h11 = _mm_load_ps((float*)&(h[1][1][i]));

    __m128 h0, h1;
    switch (codebook_idx) {
      case 0:
        h0 = _mm_add_ps(_h00, _h10);
        h1 = _mm_add_ps(_h01, _h11);
        break;
      case 1:
        h0 = _mm_sub_ps(_h00, _h10);
        h1 = _mm_sub_ps(_h01, _h11);
        break;
      case 2:
        h0 = _mm_add_ps(_h00, _MM_MULJ_PS(_h10));
        h1 = _mm_add_ps(_h01, _MM_MULJ_PS(_h11));
        break;
      case 3:
        h0 = _mm_sub_ps(_h00, _MM_MULJ_PS(_h10));
        h1 = _mm_sub_ps(_h01, _MM_MULJ_PS(_h11));
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    __m128 h0_2 = _mm_mul_ps(h0, h0);
    __m128 h1_2 = _mm_mul_ps(h1, h1);
    __m128 hh0 = _mm_add_ps(_mm_movehdup_ps(h0_2), _mm_moveldup_ps(h0_2));
    __m128 hh1 = _mm_add_ps(_mm_movehdup_ps(h1_2), _mm_moveldup_ps(h1_2));
    __m128 hh = _mm_add_ps(hh0, hh1);
    __m128 hhrec = _mm_rcp_ps(hh);

    hhrec = _mm_mul_ps(hhrec, _mm_set1_ps((float) M_SQRT2 / scaling));

    __m128 y0 = _mm_load_ps((float*)&y[0][i]);
    __m128 y1 = _mm_load_ps((float*)&y[1][i]);

    __m128 x0 = _mm_add_ps(_MM_PROD_PS(_MM_CONJ_PS(h0), y0), _MM_PROD_PS(_MM_CONJ_PS(h1), y1));
    x0 = _mm_mul_ps(hhrec, x0);

    _mm_store_ps((float*)&x[0][i], x0);

  }

  return SRSLTE_SUCCESS;
}

#endif /* LV_HAVE_SSE */

// Generic implementation of MRC 2x1 (two antennas into one layer) Spatial Multiplexing equalizer
int srslte_predecoding_multiplex_2x1_mrc_gen(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                                        cf_t *x[SRSLTE_MAX_LAYERS], int codebook_idx, int nof_symbols, float scaling) {
  float norm = (float) M_SQRT2 / scaling;

  for (int i = 0; i < nof_symbols; i += 1) {
    cf_t h0, h1;
    float hh;

    switch(codebook_idx) {
      case 0:
        h0 = h[0][0][i] + h[1][0][i];
        h1 = h[0][1][i] + h[1][1][i];
        break;
      case 1:
        h0 = h[0][0][i] - h[1][0][i];
        h1 = h[0][1][i] - h[1][1][i];
        break;
      case 2:
        h0 = h[0][0][i] + _Complex_I * h[1][0][i];
        h1 = h[0][1][i] + _Complex_I * h[1][1][i];
        break;
      case 3:
        h0 = h[0][0][i] - _Complex_I * h[1][0][i];
        h1 = h[0][1][i] - _Complex_I * h[1][1][i];
        break;
      default:
        fprintf(stderr, "Wrong codebook_idx=%d\n", codebook_idx);
        return SRSLTE_ERROR;
    }

    hh = norm / (crealf(h0) * crealf(h0) + cimagf(h0) * cimagf(h0) + crealf(h1) * crealf(h1) + cimagf(h1) * cimagf(h1));

    x[0][i] = (conjf(h0) * y[0][i] + conjf(h1) * y[1][i]) * hh;
  }
  return SRSLTE_SUCCESS;
}

int srslte_predecoding_multiplex(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], cf_t *x[SRSLTE_MAX_LAYERS],
                                 int nof_rxant, int nof_ports, int nof_layers, int codebook_idx, int nof_symbols,
                                 float scaling, float noise_estimate)
{
  if (nof_ports == 2 && nof_rxant <= 2) {
    if (nof_layers == 2) {
      switch (mimo_decoder) {
        case SRSLTE_MIMO_DECODER_ZF:
#ifdef LV_HAVE_AVX
          return srslte_predecoding_multiplex_2x2_zf_avx(y, h, x, codebook_idx, nof_symbols, scaling);
#else
#ifdef LV_HAVE_SSE
          return srslte_predecoding_multiplex_2x2_zf_sse(y, h, x, codebook_idx, nof_symbols, scaling);
#else
          return srslte_predecoding_multiplex_2x2_zf_gen(y, h, x, codebook_idx, nof_symbols, scaling);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
          break;
        case SRSLTE_MIMO_DECODER_MMSE:
#ifdef LV_HAVE_AVX
          return srslte_predecoding_multiplex_2x2_mmse_avx(y, h, x, codebook_idx, nof_symbols, scaling, noise_estimate);
#else
#ifdef LV_HAVE_SSE
          return srslte_predecoding_multiplex_2x2_mmse_sse(y, h, x, codebook_idx, nof_symbols, scaling, noise_estimate);
#else
          return srslte_predecoding_multiplex_2x2_mmse_gen(y, h, x, codebook_idx, nof_symbols, scaling, noise_estimate);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
          break;
      }
    } else {
#ifdef LV_HAVE_AVX
      return srslte_predecoding_multiplex_2x1_mrc_avx(y, h, x, codebook_idx, nof_symbols, scaling);
#else
#ifdef LV_HAVE_SSE
      return srslte_predecoding_multiplex_2x1_mrc_sse(y, h, x, codebook_idx, nof_symbols, scaling);
#else
      return srslte_predecoding_multiplex_2x1_mrc_gen(y, h, x, codebook_idx, nof_symbols, scaling);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
    }
  } else if (nof_ports == 4) {
    DEBUG("Error predecoding multiplex: not implemented for %d Tx ports", nof_ports);
  } else {
    DEBUG("Error predecoding multiplex: Invalid combination of ports %d and rx antennas %d\n", nof_ports, nof_rxant);
  }
  return SRSLTE_ERROR;
}

void srslte_predecoding_set_mimo_decoder (srslte_mimo_decoder_t _mimo_decoder) {
  mimo_decoder = _mimo_decoder;
}

/* 36.211 v10.3.0 Section 6.3.4 */
int srslte_predecoding_type(cf_t *y[SRSLTE_MAX_PORTS], cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS],
                            cf_t *x[SRSLTE_MAX_LAYERS], int nof_rxant, int nof_ports, int nof_layers,
                            int codebook_idx, int nof_symbols, srslte_mimo_type_t type, float scaling,
                            float noise_estimate) {

  if (nof_ports > SRSLTE_MAX_PORTS) {
    fprintf(stderr, "Maximum number of ports is %d (nof_ports=%d)\n", SRSLTE_MAX_PORTS,
        nof_ports);
    return -1;
  }
  if (nof_layers > SRSLTE_MAX_LAYERS) {
    fprintf(stderr, "Maximum number of layers is %d (nof_layers=%d)\n",
        SRSLTE_MAX_LAYERS, nof_layers);
    return -1;
  }

  switch (type) {
  case SRSLTE_MIMO_TYPE_CDD:
    if (nof_layers >= 2 && nof_layers <= 4) {
      switch (mimo_decoder) {
        case SRSLTE_MIMO_DECODER_ZF:
          return srslte_predecoding_ccd_zf(y, h, x, nof_rxant, nof_ports, nof_layers, nof_symbols, scaling);
          break;
        case SRSLTE_MIMO_DECODER_MMSE:
          return srslte_predecoding_ccd_mmse(y, h, x, nof_rxant, nof_ports, nof_layers, nof_symbols, scaling, noise_estimate);
          break;
      }
    } else {
      DEBUG("Invalid number of layers %d\n", nof_layers);
      return -1;
    }
    return -1; 
  case SRSLTE_MIMO_TYPE_SINGLE_ANTENNA:
    if (nof_ports == 1 && nof_layers == 1) {
      return srslte_predecoding_single_multi(y, h[0], x[0], nof_rxant, nof_symbols, scaling, noise_estimate);
    } else {
      fprintf(stderr,
          "Number of ports and layers must be 1 for transmission on single antenna ports (%d, %d)\n", nof_ports, nof_layers);
      return -1;
    }
    break;
  case SRSLTE_MIMO_TYPE_TX_DIVERSITY:
    if (nof_ports == nof_layers) {
      return srslte_predecoding_diversity_multi(y, h, x, nof_rxant, nof_ports, nof_symbols, scaling);
    } else {
      fprintf(stderr,
          "Error number of layers must equal number of ports in transmit diversity\n");
      return -1;
    }
    break;
  case SRSLTE_MIMO_TYPE_SPATIAL_MULTIPLEX:
    return srslte_predecoding_multiplex(y, h, x, nof_rxant, nof_ports, nof_layers, codebook_idx, nof_symbols,
                                        scaling, noise_estimate);
    default:
      return SRSLTE_ERROR;
  }
  return SRSLTE_ERROR;
}






/************************************************
 * 
 * TRANSMITTER SIDE FUNCTIONS
 * 
 **************************************************/

int srslte_precoding_single(cf_t *x, cf_t *y, int nof_symbols, float scaling) {
  if (scaling == 1.0f) {
    memcpy(y, x, nof_symbols * sizeof(cf_t));
  } else {
    srslte_vec_sc_prod_cfc(x, scaling, y, (uint32_t) nof_symbols);
  }
  return nof_symbols;
}
int srslte_precoding_diversity(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_ports,
    int nof_symbols, float scaling) {
  int i;
  if (nof_ports == 2) {
    for (i = 0; i < nof_symbols; i++) {
      y[0][2 * i] = x[0][i];
      y[1][2 * i] = -conjf(x[1][i]);
      y[0][2 * i + 1] = x[1][i];
      y[1][2 * i + 1] = conjf(x[0][i]);
    }
    // normalize
    srslte_vec_sc_prod_cfc(y[0], scaling/sqrtf(2), y[0], 2*nof_symbols);
    srslte_vec_sc_prod_cfc(y[1], scaling/sqrtf(2), y[1], 2*nof_symbols);
    return 2 * i;
  } else if (nof_ports == 4) {
    scaling /=  sqrtf(2);

    //int m_ap = (nof_symbols%4)?(nof_symbols*4-2):nof_symbols*4;
    int m_ap = 4 * nof_symbols;
    for (i = 0; i < m_ap / 4; i++) {
      y[0][4 * i] = x[0][i] * scaling;
      y[1][4 * i] = 0;
      y[2][4 * i] = -conjf(x[1][i]) * scaling;
      y[3][4 * i] = 0;

      y[0][4 * i + 1] = x[1][i] * scaling;
      y[1][4 * i + 1] = 0;
      y[2][4 * i + 1] = conjf(x[0][i]) * scaling;
      y[3][4 * i + 1] = 0;

      y[0][4 * i + 2] = 0;
      y[1][4 * i + 2] = x[2][i] * scaling;
      y[2][4 * i + 2] = 0;
      y[3][4 * i + 2] = -conjf(x[3][i]) * scaling;

      y[0][4 * i + 3] = 0;
      y[1][4 * i + 3] = x[3][i] * scaling;
      y[2][4 * i + 3] = 0;
      y[3][4 * i + 3] = conjf(x[2][i]) * scaling;
    }
    return 4 * i;
  } else {
    fprintf(stderr, "Number of ports must be 2 or 4 for transmit diversity (nof_ports=%d)\n", nof_ports);
    return -1;
  }
}

#ifdef LV_HAVE_AVX

int srslte_precoding_cdd_2x2_avx(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_symbols, float scaling)
{
  __m256 norm_avx = _mm256_set1_ps(0.5f * scaling);
  for (int i = 0; i < nof_symbols - 3; i += 4) {
    __m256 x0 = _mm256_load_ps((float*) &x[0][i]);
    __m256 x1 = _mm256_load_ps((float*) &x[1][i]);

    __m256 y0 = _mm256_mul_ps(norm_avx, _mm256_add_ps(x0, x1));

    x0 = _mm256_xor_ps(x0, _mm256_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f));
    x1 = _mm256_xor_ps(x1, _mm256_set_ps(+0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, -0.0f, -0.0f));

    __m256 y1 = _mm256_mul_ps(norm_avx, _mm256_add_ps(x0, x1));

    _mm256_store_ps((float*)&y[0][i], y0);
    _mm256_store_ps((float*)&y[1][i], y1);
  }

  return 2*nof_symbols;
}

#endif /* LV_HAVE_AVX */

#ifdef LV_HAVE_SSE

int srslte_precoding_cdd_2x2_sse(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_symbols, float scaling)
{
  __m128 norm_sse = _mm_set1_ps(0.5f * scaling);
  for (int i = 0; i < nof_symbols - 1; i += 2) {
    __m128 x0 = _mm_load_ps((float*) &x[0][i]);
    __m128 x1 = _mm_load_ps((float*) &x[1][i]);

    __m128 y0 = _mm_mul_ps(norm_sse, _mm_add_ps(x0, x1));

    x0 = _mm_xor_ps(x0, _mm_setr_ps(+0.0f, +0.0f, -0.0f, -0.0f));
    x1 = _mm_xor_ps(x1, _mm_set_ps(+0.0f, +0.0f, -0.0f, -0.0f));

    __m128 y1 = _mm_mul_ps(norm_sse, _mm_add_ps(x0, x1));

    _mm_store_ps((float*)&y[0][i], y0);
    _mm_store_ps((float*)&y[1][i], y1);
  }

  return 2 * nof_symbols;
}

#endif /* LV_HAVE_SSE */


int srslte_precoding_cdd_2x2_gen(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_symbols, float scaling)
{
  scaling /= 2.0f;
  for (int i = 0; i < nof_symbols; i++) {
    y[0][i] =  (x[0][i]+x[1][i]) * scaling;
    y[1][i] =  (x[0][i]-x[1][i]) * scaling;
    i++;
    y[0][i] =  (x[0][i]+x[1][i]) * scaling;
    y[1][i] = (-x[0][i]+x[1][i]) * scaling;
  }
  return 2 * nof_symbols;
}

int srslte_precoding_cdd(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_layers, int nof_ports, int nof_symbols, float scaling)
{
  if (nof_ports == 2) {
    if (nof_layers != 2) {
      DEBUG("Invalid number of layers %d for 2 ports\n", nof_layers);
      return -1;
    }
#ifdef LV_HAVE_AVX
    return srslte_precoding_cdd_2x2_avx(x, y, nof_symbols, scaling);
#else
#ifdef LV_HAVE_SSE
    return srslte_precoding_cdd_2x2_sse(x, y, nof_symbols, scaling);
#else
    return srslte_precoding_cdd_2x2_gen(x, y, nof_symbols, scaling);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */
  } else if (nof_ports == 4) {
    DEBUG("Not implemented\n");
    return -1;
  } else {
    DEBUG("Number of ports must be 2 or 4 for transmit diversity (nof_ports=%d)\n", nof_ports);
    return -1;
  }
}

int srslte_precoding_multiplex(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_layers, int nof_ports,
                               int codebook_idx, uint32_t nof_symbols, float scaling)
{
  int i = 0;
  if (nof_ports == 2) {
    if (nof_layers == 1) {
      scaling /= sqrtf(2.0f);
      switch(codebook_idx) {
        case 0:
          srslte_vec_sc_prod_cfc(x[0], scaling, y[0], nof_symbols);
          srslte_vec_sc_prod_cfc(x[0], scaling, y[1], nof_symbols);
          break;
        case 1:
          srslte_vec_sc_prod_cfc(x[0], scaling, y[0], nof_symbols);
          srslte_vec_sc_prod_cfc(x[0], -scaling, y[1], nof_symbols);
          break;
        case 2:
          srslte_vec_sc_prod_cfc(x[0], scaling, y[0], nof_symbols);
          srslte_vec_sc_prod_ccc(x[0], _Complex_I * scaling, y[1], nof_symbols);
          break;
        case 3:
          srslte_vec_sc_prod_cfc(x[0], scaling, y[0], nof_symbols);
          srslte_vec_sc_prod_ccc(x[0], -_Complex_I * scaling, y[1], nof_symbols);
          break;
        default:
          fprintf(stderr, "Invalid multiplex combination: codebook_idx=%d, nof_layers=%d, nof_ports=%d\n",
                  codebook_idx, nof_layers, nof_ports);
          return SRSLTE_ERROR;
      }
    } else if (nof_layers == 2) {
      switch(codebook_idx) {
        case 0:
          scaling /= sqrtf(2.0f);
          srslte_vec_sc_prod_cfc(x[0], scaling, y[0], nof_symbols);
          srslte_vec_sc_prod_cfc(x[1], scaling, y[1], nof_symbols);
          break;
        case 1:
          scaling /= 2.0f;
#ifdef LV_HAVE_AVX
          for (; i < nof_symbols - 3; i += 4) {
            __m256 x0 = _mm256_load_ps((float *) &x[0][i]);
            __m256 x1 = _mm256_load_ps((float *) &x[1][i]);

            __m256 y0 = _mm256_mul_ps(_mm256_set1_ps(scaling), _mm256_add_ps(x0, x1));
            __m256 y1 = _mm256_mul_ps(_mm256_set1_ps(scaling), _mm256_sub_ps(x0, x1));

            _mm256_store_ps((float *) &y[0][i], y0);
            _mm256_store_ps((float *) &y[1][i], y1);
          }
#endif /* LV_HAVE_AVX */

#ifdef LV_HAVE_SSE
          for (; i < nof_symbols - 1; i += 2) {
            __m128 x0 = _mm_load_ps((float *) &x[0][i]);
            __m128 x1 = _mm_load_ps((float *) &x[1][i]);

            __m128 y0 = _mm_mul_ps(_mm_set1_ps(scaling), _mm_add_ps(x0, x1));
            __m128 y1 = _mm_mul_ps(_mm_set1_ps(scaling), _mm_sub_ps(x0, x1));

            _mm_store_ps((float *) &y[0][i], y0);
            _mm_store_ps((float *) &y[1][i], y1);
          }
#endif /* LV_HAVE_SSE */

          for (; i < nof_symbols; i++) {
            y[0][i] = (x[0][i] + x[1][i]) * scaling;
            y[1][i] = (x[0][i] - x[1][i]) * scaling;
          }
          break;
        case 2:
          scaling /= 2.0f;
#ifdef LV_HAVE_AVX
          for (; i < nof_symbols - 3; i += 4) {
            __m256 x0 = _mm256_load_ps((float*)&x[0][i]);
            __m256 x1 = _mm256_load_ps((float*)&x[1][i]);

            __m256 y0 = _mm256_mul_ps(_mm256_set1_ps(scaling), _mm256_add_ps(x0, x1));
            __m256 y1 = _mm256_mul_ps(_mm256_set1_ps(scaling), _MM256_MULJ_PS(_mm256_sub_ps(x0, x1)));

            _mm256_store_ps((float*)&y[0][i], y0);
            _mm256_store_ps((float*)&y[1][i], y1);
          }
#endif /* LV_HAVE_AVX */

#ifdef LV_HAVE_SSE
          for (; i < nof_symbols - 1; i += 2) {
            __m128 x0 = _mm_load_ps((float*)&x[0][i]);
            __m128 x1 = _mm_load_ps((float*)&x[1][i]);

            __m128 y0 = _mm_mul_ps(_mm_set1_ps(scaling), _mm_add_ps(x0, x1));
            __m128 y1 = _mm_mul_ps(_mm_set1_ps(scaling), _MM_MULJ_PS(_mm_sub_ps(x0, x1)));

            _mm_store_ps((float*)&y[0][i], y0);
            _mm_store_ps((float*)&y[1][i], y1);
          }
#endif /* LV_HAVE_SSE */

          for (; i < nof_symbols; i++) {
            y[0][i] = (x[0][i] + x[1][i])*scaling;
            y[1][i] = (_Complex_I*x[0][i] - _Complex_I*x[1][i])*scaling;
          }
          break;
        case 3:
        default:
          fprintf(stderr, "Invalid multiplex combination: codebook_idx=%d, nof_layers=%d, nof_ports=%d\n",
                  codebook_idx, nof_layers, nof_ports);
          return SRSLTE_ERROR;
      }
    } else {
      ERROR("Not implemented");
    }
  } else {
    ERROR("Not implemented");
  }
  return SRSLTE_SUCCESS;
}

/* 36.211 v10.3.0 Section 6.3.4 */
int srslte_precoding_type(cf_t *x[SRSLTE_MAX_LAYERS], cf_t *y[SRSLTE_MAX_PORTS], int nof_layers,
    int nof_ports, int codebook_idx, int nof_symbols, float scaling, srslte_mimo_type_t type) {

  if (nof_ports > SRSLTE_MAX_PORTS) {
    fprintf(stderr, "Maximum number of ports is %d (nof_ports=%d)\n", SRSLTE_MAX_PORTS,
        nof_ports);
    return -1;
  }
  if (nof_layers > SRSLTE_MAX_LAYERS) {
    fprintf(stderr, "Maximum number of layers is %d (nof_layers=%d)\n",
        SRSLTE_MAX_LAYERS, nof_layers);
    return -1;
  }

  switch (type) {
    case SRSLTE_MIMO_TYPE_CDD:
      return srslte_precoding_cdd(x, y, nof_layers, nof_ports, nof_symbols, scaling);
    case SRSLTE_MIMO_TYPE_SINGLE_ANTENNA:
      if (nof_ports == 1 && nof_layers == 1) {
        return srslte_precoding_single(x[0], y[0], nof_symbols, scaling);
      } else {
        fprintf(stderr,
                "Number of ports and layers must be 1 for transmission on single antenna ports\n");
        return -1;
      }
      break;
    case SRSLTE_MIMO_TYPE_TX_DIVERSITY:
      if (nof_ports == nof_layers) {
        return srslte_precoding_diversity(x, y, nof_ports, nof_symbols, scaling);
      } else {
        fprintf(stderr,
                "Error number of layers must equal number of ports in transmit diversity\n");
        return -1;
      }
    case SRSLTE_MIMO_TYPE_SPATIAL_MULTIPLEX:
      return srslte_precoding_multiplex(x, y, nof_layers, nof_ports, codebook_idx, (uint32_t) nof_symbols, scaling);
    default:
      return SRSLTE_ERROR;
  }
  return SRSLTE_ERROR;
}

#define PMI_SEL_PRECISION 24

/* PMI Select for 1 layer */
int srslte_precoding_pmi_select_1l_gen(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {

#define SQRT1_2 ((float)M_SQRT1_2)
  float max_sinr = 0.0;
  uint32_t i, count;

  for (i = 0; i < 4; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols; j += PMI_SEL_PRECISION) {
      /* 0. Load channel matrix */
      cf_t h00 = h[0][0][j];
      cf_t h01 = h[1][0][j];
      cf_t h10 = h[0][1][j];
      cf_t h11 = h[1][1][j];

      /* 1. B = W'* H' */
      cf_t a0, a1;
      switch (i) {
        case 0:
          a0 = conjf(h00) + conjf(h01);
          a1 = conjf(h10) + conjf(h11);
          break;
        case 1:
          a0 = conjf(h00) - conjf(h01);
          a1 = conjf(h10) - conjf(h11);
          break;
        case 2:
          a0 = conjf(h00) - _Complex_I * conjf(h01);
          a1 = conjf(h10) - _Complex_I * conjf(h11);
          break;
        case 3:
          a0 = conjf(h00) + _Complex_I * conjf(h01);
          a1 = conjf(h10) + _Complex_I * conjf(h11);
          break;
      }
      a0 *= SQRT1_2;
      a1 *= SQRT1_2;

      /* 2. B = W' * H' * H = A * H */
      cf_t b0 = a0 * h00 + a1 * h10;
      cf_t b1 = a0 * h01 + a1 * h11;

      /* 3. C = W' * H' * H * W' = B * W */
      cf_t c;
      switch (i) {
        case 0:
          c = b0 + b1;
          break;
        case 1:
          c = b0 - b1;
          break;
        case 2:
          c = b0 + _Complex_I * b1;
          break;
        case 3:
          c = b0 - _Complex_I * b1;
          break;
        default:
          return SRSLTE_ERROR;
      }
      c *= SQRT1_2;

      /* Add for averaging */
      sinr_list[i] += crealf(c);

      count++;
    }

    /* Divide average by noise */
    sinr_list[i] /= noise_estimate * count;

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#ifdef LV_HAVE_SSE

/* PMI Select for 1 layer */
int srslte_precoding_pmi_select_1l_sse(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {
  float max_sinr = 0.0;
  uint32_t i, count;
  __m128 sse_norm = _mm_set1_ps(0.5f);

  for (i = 0; i < 4; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols - PMI_SEL_PRECISION * 2 + 1; j += PMI_SEL_PRECISION * 2) {
      /* 0. Load channel matrix */
      __m128 h00 = _mm_set_ps(crealf(h[0][0][j]),
                              cimagf(h[0][0][j]),
                              crealf(h[0][0][j + PMI_SEL_PRECISION]),
                              cimagf(h[0][0][j + PMI_SEL_PRECISION]));
      __m128 h01 = _mm_set_ps(crealf(h[1][0][j]),
                              cimagf(h[1][0][j]),
                              crealf(h[1][0][j + PMI_SEL_PRECISION]),
                              cimagf(h[1][0][j + PMI_SEL_PRECISION]));
      __m128 h10 = _mm_set_ps(crealf(h[0][1][j]),
                              cimagf(h[0][1][j]),
                              crealf(h[0][1][j + PMI_SEL_PRECISION]),
                              cimagf(h[0][1][j + PMI_SEL_PRECISION]));
      __m128 h11 = _mm_set_ps(crealf(h[1][1][j]),
                              cimagf(h[1][1][j]),
                              crealf(h[1][1][j + PMI_SEL_PRECISION]),
                              cimagf(h[1][1][j + PMI_SEL_PRECISION]));

      /* 1. B = W'* H' */
      __m128 a0, a1;
      switch (i) {
        case 0:
          a0 = _mm_add_ps(_MM_CONJ_PS(h00), _MM_CONJ_PS(h01));
          a1 = _mm_add_ps(_MM_CONJ_PS(h10), _MM_CONJ_PS(h11));
          break;
        case 1:
          a0 = _mm_sub_ps(_MM_CONJ_PS(h00), _MM_CONJ_PS(h01));
          a1 = _mm_sub_ps(_MM_CONJ_PS(h10), _MM_CONJ_PS(h11));
          break;
        case 2:
          a0 = _mm_add_ps(_MM_CONJ_PS(h00), _MM_MULJ_PS(_MM_CONJ_PS(h01)));
          a1 = _mm_add_ps(_MM_CONJ_PS(h10), _MM_MULJ_PS(_MM_CONJ_PS(h11)));
          break;
        case 3:
          a0 = _mm_sub_ps(_MM_CONJ_PS(h00), _MM_MULJ_PS(_MM_CONJ_PS(h01)));
          a1 = _mm_sub_ps(_MM_CONJ_PS(h10), _MM_MULJ_PS(_MM_CONJ_PS(h11)));
          break;
      }

      /* 2. B = W' * H' * H = A * H */
      __m128 b0 = _mm_add_ps(_MM_PROD_PS(a0, h00), _MM_PROD_PS(a1, h10));
      __m128 b1 = _mm_add_ps(_MM_PROD_PS(a0, h01), _MM_PROD_PS(a1, h11));

      /* 3. C = W' * H' * H * W' = B * W */
      __m128 c;
      switch (i) {
        case 0:
          c = _mm_add_ps(b0, b1);
          break;
        case 1:
          c = _mm_sub_ps(b0, b1);
          break;
        case 2:
          c = _mm_sub_ps(b0, _MM_MULJ_PS(b1));
          break;
        case 3:
          c = _mm_add_ps(b0, _MM_MULJ_PS(b1));
          break;
        default:
          return SRSLTE_ERROR;
      }
      c = _mm_mul_ps(c, sse_norm);

      /* Add for averaging */
      __attribute__((aligned(128))) float gamma[4];
      _mm_store_ps(gamma, c);
      sinr_list[i] += gamma[0] + gamma[2];

      count += 2;
    }

    /* Divide average by noise */
    sinr_list[i] /= noise_estimate * count;

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#endif /* LV_HAVE_SSE */

#ifdef LV_HAVE_AVX

/* PMI Select for 1 layer */
int srslte_precoding_pmi_select_1l_avx(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {
  float max_sinr = 0.0;
  uint32_t i, count;
  __m256 avx_norm = _mm256_set1_ps(0.5f);

  for (i = 0; i < 4; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols - PMI_SEL_PRECISION * 4 + 1; j += PMI_SEL_PRECISION * 4) {
      /* 0. Load channel matrix */
      __m256 h00 = _mm256_setr_ps(crealf(h[0][0][j]),
                                 cimagf(h[0][0][j]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION * 3]));
      __m256 h01 = _mm256_setr_ps(crealf(h[1][0][j]),
                                 cimagf(h[1][0][j]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION * 3]));
      __m256 h10 = _mm256_setr_ps(crealf(h[0][1][j]),
                                 cimagf(h[0][1][j]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION * 3]));
      __m256 h11 = _mm256_setr_ps(crealf(h[1][1][j]),
                                 cimagf(h[1][1][j]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION * 3]));

      /* 1. B = W'* H' */
      __m256 a0, a1;
      switch (i) {
        case 0:
          a0 = _mm256_add_ps(_MM256_CONJ_PS(h00), _MM256_CONJ_PS(h01));
          a1 = _mm256_add_ps(_MM256_CONJ_PS(h10), _MM256_CONJ_PS(h11));
          break;
        case 1:
          a0 = _mm256_sub_ps(_MM256_CONJ_PS(h00), _MM256_CONJ_PS(h01));
          a1 = _mm256_sub_ps(_MM256_CONJ_PS(h10), _MM256_CONJ_PS(h11));
          break;
        case 2:
          a0 = _mm256_sub_ps(_MM256_CONJ_PS(h00), _MM256_MULJ_PS(_MM256_CONJ_PS(h01)));
          a1 = _mm256_sub_ps(_MM256_CONJ_PS(h10), _MM256_MULJ_PS(_MM256_CONJ_PS(h11)));
          break;
        default:
          a0 = _mm256_add_ps(_MM256_CONJ_PS(h00), _MM256_MULJ_PS(_MM256_CONJ_PS(h01)));
          a1 = _mm256_add_ps(_MM256_CONJ_PS(h10), _MM256_MULJ_PS(_MM256_CONJ_PS(h11)));
          break;
      }

      /* 2. B = W' * H' * H = A * H */
#ifdef  LV_HAVE_FMA
      __m256 b0 = _MM256_PROD_ADD_PS(a0, h00, _MM256_PROD_PS(a1, h10));
      __m256 b1 = _MM256_PROD_ADD_PS(a0, h01, _MM256_PROD_PS(a1, h11));
#else
      __m256 b0 = _mm256_add_ps(_MM256_PROD_PS(a0, h00), _MM256_PROD_PS(a1, h10));
      __m256 b1 = _mm256_add_ps(_MM256_PROD_PS(a0, h01), _MM256_PROD_PS(a1, h11));
#endif /* LV_HAVE_FMA */

      /* 3. C = W' * H' * H * W' = B * W */
      __m256 c;
      switch (i) {
        case 0:
          c = _mm256_add_ps(b0, b1);
          break;
        case 1:
          c = _mm256_sub_ps(b0, b1);
          break;
        case 2:
          c = _mm256_add_ps(b0, _MM256_MULJ_PS(b1));
          break;
        case 3:
          c = _mm256_sub_ps(b0, _MM256_MULJ_PS(b1));
          break;
        default:
          return SRSLTE_ERROR;
      }
      c = _mm256_mul_ps(c, avx_norm);

      /* Add for averaging */
      __attribute__((aligned(256))) float gamma[8];
      _mm256_store_ps(gamma, c);
      sinr_list[i] += gamma[0] + gamma[2] + gamma[4] + gamma[6];

      count += 4;
    }

    /* Divide average by noise */
    sinr_list[i] /= noise_estimate * count;

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#endif /* LV_HAVE_AVX */

int srslte_precoding_pmi_select_1l(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                   float noise_estimate, uint32_t *pmi,
                                   float sinr_list[SRSLTE_MAX_CODEBOOKS]) {
  int ret;
#ifdef LV_HAVE_AVX
  ret = srslte_precoding_pmi_select_1l_avx(h, nof_symbols, noise_estimate, pmi, sinr_list);
#else
  #ifdef LV_HAVE_SSE
  ret = srslte_precoding_pmi_select_1l_sse(h, nof_symbols, noise_estimate, pmi, sinr_list);
#else
  ret = srslte_precoding_pmi_select_1l_gen(h, nof_symbols, noise_estimate, pmi, sinr_list);
#endif
#endif
  INFO("Precoder PMI Select for 1 layer SINR=[%.1fdB; %.1fdB; %.1fdB; %.1fdB] PMI=%d\n", 10 * log10(sinr_list[0]),
       10 * log10(sinr_list[1]), 10 * log10(sinr_list[2]), 10 * log10(sinr_list[3]), *pmi);

  return ret;
}

int srslte_precoding_pmi_select_2l_gen(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {

  float max_sinr = 0.0;
  uint32_t i, count;

  for (i = 0; i < 2; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols; j += PMI_SEL_PRECISION) {
      /* 0. Load channel matrix */
      cf_t h00 = h[0][0][j];
      cf_t h01 = h[1][0][j];
      cf_t h10 = h[0][1][j];
      cf_t h11 = h[1][1][j];

      /* 1. B = W'* H' */
      cf_t a00, a01, a10, a11;
      switch (i) {
        case 0:
          a00 = conjf(h00) + conjf(h01);
          a01 = conjf(h10) + conjf(h11);
          a10 = conjf(h00) - conjf(h01);
          a11 = conjf(h10) - conjf(h11);
          break;
        case 1:
          a00 = conjf(h00) - _Complex_I * conjf(h01);
          a01 = conjf(h10) - _Complex_I * conjf(h11);
          a10 = conjf(h00) + _Complex_I * conjf(h01);
          a11 = conjf(h10) + _Complex_I * conjf(h11);
          break;
        default:
          return SRSLTE_ERROR;
      }

      /* 2. B = W' * H' * H = A * H */
      cf_t b00 = a00 * h00 + a01 * h10;
      cf_t b01 = a00 * h01 + a01 * h11;
      cf_t b10 = a10 * h00 + a11 * h10;
      cf_t b11 = a10 * h01 + a11 * h11;

      /* 3. C = W' * H' * H * W' = B * W */
      cf_t c00, c01, c10, c11;
      switch (i) {
        case 0:
          c00 = b00 + b01;
          c01 = b00 - b01;
          c10 = b10 + b11;
          c11 = b10 - b11;
          break;
        case 1:
          c00 = b00 + _Complex_I * b01;
          c01 = b00 - _Complex_I * b01;
          c10 = b10 + _Complex_I * b11;
          c11 = b10 - _Complex_I * b11;
          break;
        default:
          return SRSLTE_ERROR;
      }
      c00 *= 0.25;
      c01 *= 0.25;
      c10 *= 0.25;
      c11 *= 0.25;

      /* 4. C += noise * I */
      c00 += noise_estimate;
      c11 += noise_estimate;

      /* 5. detC */
      cf_t detC = c00 * c11 - c01 * c10;
      cf_t inv_detC = conjf(detC) / (crealf(detC) * crealf(detC) + cimagf(detC) * cimagf(detC));

      cf_t den0 = noise_estimate * c00 * inv_detC;
      cf_t den1 = noise_estimate * c11 * inv_detC;

      float gamma0 = crealf((conjf(den0) / (crealf(den0) * crealf(den0) + cimagf(den0) * cimagf(den0))) - 1);
      float gamma1 = crealf((conjf(den1) / (crealf(den1) * crealf(den1) + cimagf(den1) * cimagf(den1))) - 1);

      /* Add for averaging */
      sinr_list[i] += (gamma0 + gamma1);

      count++;
    }

    /* Divide average by noise */
    if (count) {
      sinr_list[i] /= count;
    }

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#ifdef LV_HAVE_SSE

int srslte_precoding_pmi_select_2l_sse(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {

  float max_sinr = 0.0;
  uint32_t i, count;

  __m128 sse_noise_estimate = _mm_setr_ps(noise_estimate, 0.0f, noise_estimate, 0.0f);
  __m128 sse_norm = _mm_set1_ps(0.25f);
  __m128 sse_ones = _mm_set1_ps(1.0f);

  for (i = 0; i < 2; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols - PMI_SEL_PRECISION * 2 + 1; j += PMI_SEL_PRECISION * 2) {
      /* 0. Load channel matrix */
      __m128 h00 = _mm_setr_ps(crealf(h[0][0][j]),
                               cimagf(h[0][0][j]),
                               crealf(h[0][0][j + PMI_SEL_PRECISION]),
                               cimagf(h[0][0][j + PMI_SEL_PRECISION]));
      __m128 h01 = _mm_setr_ps(crealf(h[1][0][j]),
                               cimagf(h[1][0][j]),
                               crealf(h[1][0][j + PMI_SEL_PRECISION]),
                               cimagf(h[1][0][j + PMI_SEL_PRECISION]));
      __m128 h10 = _mm_setr_ps(crealf(h[0][1][j]),
                               cimagf(h[0][1][j]),
                               crealf(h[0][1][j + PMI_SEL_PRECISION]),
                               cimagf(h[0][1][j + PMI_SEL_PRECISION]));
      __m128 h11 = _mm_setr_ps(crealf(h[1][1][j]),
                               cimagf(h[1][1][j]),
                               crealf(h[1][1][j + PMI_SEL_PRECISION]),
                               cimagf(h[1][1][j + PMI_SEL_PRECISION]));

      /* 1. B = W'* H' */
      __m128 a00, a01, a10, a11;
      switch (i) {
        case 0:
          a00 = _mm_add_ps(_MM_CONJ_PS(h00), _MM_CONJ_PS(h01));
          a01 = _mm_add_ps(_MM_CONJ_PS(h10), _MM_CONJ_PS(h11));
          a10 = _mm_sub_ps(_MM_CONJ_PS(h00), _MM_CONJ_PS(h01));
          a11 = _mm_sub_ps(_MM_CONJ_PS(h10), _MM_CONJ_PS(h11));
          break;
        case 1:
          a00 = _mm_sub_ps(_MM_CONJ_PS(h00), _MM_MULJ_PS(_MM_CONJ_PS(h01)));
          a01 = _mm_sub_ps(_MM_CONJ_PS(h10), _MM_MULJ_PS(_MM_CONJ_PS(h11)));
          a10 = _mm_add_ps(_MM_CONJ_PS(h00), _MM_MULJ_PS(_MM_CONJ_PS(h01)));
          a11 = _mm_add_ps(_MM_CONJ_PS(h10), _MM_MULJ_PS(_MM_CONJ_PS(h11)));
          break;
        default:
          return SRSLTE_ERROR;
      }

      /* 2. B = W' * H' * H = A * H */
      __m128 b00 = _mm_add_ps(_MM_PROD_PS(a00, h00), _MM_PROD_PS(a01, h10));
      __m128 b01 = _mm_add_ps(_MM_PROD_PS(a00, h01), _MM_PROD_PS(a01, h11));
      __m128 b10 = _mm_add_ps(_MM_PROD_PS(a10, h00), _MM_PROD_PS(a11, h10));
      __m128 b11 = _mm_add_ps(_MM_PROD_PS(a10, h01), _MM_PROD_PS(a11, h11));

      /* 3. C = W' * H' * H * W' = B * W */
      __m128 c00, c01, c10, c11;
      switch (i) {
        case 0:
          c00 = _mm_add_ps(b00, b01);
          c01 = _mm_sub_ps(b00, b01);
          c10 = _mm_add_ps(b10, b11);
          c11 = _mm_sub_ps(b10, b11);
          break;
        case 1:
          c00 = _mm_add_ps(b00, _MM_MULJ_PS(b01));
          c01 = _mm_sub_ps(b00, _MM_MULJ_PS(b01));
          c10 = _mm_add_ps(b10, _MM_MULJ_PS(b11));
          c11 = _mm_sub_ps(b10, _MM_MULJ_PS(b11));
          break;
        default:
          return SRSLTE_ERROR;
      }
      c00 = _mm_mul_ps(c00, sse_norm);
      c01 = _mm_mul_ps(c01, sse_norm);
      c10 = _mm_mul_ps(c10, sse_norm);
      c11 = _mm_mul_ps(c11, sse_norm);

      /* 4. C += noise * I */
      c00 = _mm_add_ps(c00, sse_noise_estimate);
      c11 = _mm_add_ps(c11, sse_noise_estimate);

      /* 5. detC */
      __m128 detC = srslte_mat_2x2_det_sse(c00, c01, c10, c11);
      __m128 inv_detC = srslte_mat_cf_recip_sse(detC);
      inv_detC = _mm_mul_ps(sse_noise_estimate, inv_detC);

      __m128 den0 = _MM_PROD_PS(c00, inv_detC);
      __m128 den1 = _MM_PROD_PS(c11, inv_detC);

      __m128 gamma0 = _mm_sub_ps(_mm_rcp_ps(den0), sse_ones);
      __m128 gamma1 = _mm_sub_ps(_mm_rcp_ps(den1), sse_ones);

      /* Add for averaging */
      __m128 sinr_sse = _mm_add_ps(gamma0, gamma1);
      __attribute__((aligned(128))) float sinr[4];
      _mm_store_ps(sinr, sinr_sse);

      sinr_list[i] += sinr[0] + sinr[2];

      count += 2;
    }

    /* Divide average by noise */
    if (count) {
      sinr_list[i] /= count;
    }

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#endif /* LV_HAVE_SSE */

#ifdef LV_HAVE_AVX

int srslte_precoding_pmi_select_2l_avx(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                       float noise_estimate, uint32_t *pmi,
                                       float sinr_list[SRSLTE_MAX_CODEBOOKS]) {

  float max_sinr = 0.0;
  uint32_t i, count;

  __m256 avx_noise_estimate = _mm256_setr_ps(noise_estimate, 0.0f, noise_estimate, 0.0f,
                                        noise_estimate, 0.0f, noise_estimate, 0.0f);
  __m256 avx_norm = _mm256_set1_ps(0.25f);
  __m256 avx_ones = _mm256_set1_ps(1.0f);

  for (i = 0; i < 2; i++) {
    sinr_list[i] = 0;
    count = 0;

    for (uint32_t j = 0; j < nof_symbols - PMI_SEL_PRECISION * 4 + 1; j += PMI_SEL_PRECISION * 4) {
      /* 0. Load channel matrix */
      __m256 h00 = _mm256_setr_ps(crealf(h[0][0][j]),
                                 cimagf(h[0][0][j]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[0][0][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[0][0][j + PMI_SEL_PRECISION * 3]));
      __m256 h01 = _mm256_setr_ps(crealf(h[1][0][j]),
                                 cimagf(h[1][0][j]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[1][0][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[1][0][j + PMI_SEL_PRECISION * 3]));
      __m256 h10 = _mm256_setr_ps(crealf(h[0][1][j]),
                                 cimagf(h[0][1][j]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[0][1][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[0][1][j + PMI_SEL_PRECISION * 3]));
      __m256 h11 = _mm256_setr_ps(crealf(h[1][1][j]),
                                 cimagf(h[1][1][j]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION * 2]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION * 2]),
                                 crealf(h[1][1][j + PMI_SEL_PRECISION * 3]),
                                 cimagf(h[1][1][j + PMI_SEL_PRECISION * 3]));

      /* 1. B = W'* H' */
      __m256 a00, a01, a10, a11;
      switch (i) {
        case 0:
          a00 = _mm256_add_ps(_MM256_CONJ_PS(h00), _MM256_CONJ_PS(h01));
          a01 = _mm256_add_ps(_MM256_CONJ_PS(h10), _MM256_CONJ_PS(h11));
          a10 = _mm256_sub_ps(_MM256_CONJ_PS(h00), _MM256_CONJ_PS(h01));
          a11 = _mm256_sub_ps(_MM256_CONJ_PS(h10), _MM256_CONJ_PS(h11));
          break;
        case 1:
          a00 = _mm256_sub_ps(_MM256_CONJ_PS(h00), _MM256_MULJ_PS(_MM256_CONJ_PS(h01)));
          a01 = _mm256_sub_ps(_MM256_CONJ_PS(h10), _MM256_MULJ_PS(_MM256_CONJ_PS(h11)));
          a10 = _mm256_add_ps(_MM256_CONJ_PS(h00), _MM256_MULJ_PS(_MM256_CONJ_PS(h01)));
          a11 = _mm256_add_ps(_MM256_CONJ_PS(h10), _MM256_MULJ_PS(_MM256_CONJ_PS(h11)));
          break;
        default:
          return SRSLTE_ERROR;
      }

      /* 2. B = W' * H' * H = A * H */
#ifdef LV_HAVE_FMA
      __m256 b00 = _MM256_PROD_ADD_PS(a00, h00, _MM256_PROD_PS(a01, h10));
      __m256 b01 = _MM256_PROD_ADD_PS(a00, h01, _MM256_PROD_PS(a01, h11));
      __m256 b10 = _MM256_PROD_ADD_PS(a10, h00, _MM256_PROD_PS(a11, h10));
      __m256 b11 = _MM256_PROD_ADD_PS(a10, h01, _MM256_PROD_PS(a11, h11));
#else
      __m256 b00 = _mm256_add_ps(_MM256_PROD_PS(a00, h00), _MM256_PROD_PS(a01, h10));
      __m256 b01 = _mm256_add_ps(_MM256_PROD_PS(a00, h01), _MM256_PROD_PS(a01, h11));
      __m256 b10 = _mm256_add_ps(_MM256_PROD_PS(a10, h00), _MM256_PROD_PS(a11, h10));
      __m256 b11 = _mm256_add_ps(_MM256_PROD_PS(a10, h01), _MM256_PROD_PS(a11, h11));
#endif /* LV_HAVE_FMA */

      /* 3. C = W' * H' * H * W' = B * W */
      __m256 c00, c01, c10, c11;
      switch (i) {
        case 0:
          c00 = _mm256_add_ps(b00, b01);
          c01 = _mm256_sub_ps(b00, b01);
          c10 = _mm256_add_ps(b10, b11);
          c11 = _mm256_sub_ps(b10, b11);
          break;
        case 1:
          c00 = _mm256_add_ps(b00, _MM256_MULJ_PS(b01));
          c01 = _mm256_sub_ps(b00, _MM256_MULJ_PS(b01));
          c10 = _mm256_add_ps(b10, _MM256_MULJ_PS(b11));
          c11 = _mm256_sub_ps(b10, _MM256_MULJ_PS(b11));
          break;
        default:
          return SRSLTE_ERROR;
      }
      c00 = _mm256_mul_ps(c00, avx_norm);
      c01 = _mm256_mul_ps(c01, avx_norm);
      c10 = _mm256_mul_ps(c10, avx_norm);
      c11 = _mm256_mul_ps(c11, avx_norm);

      /* 4. C += noise * I */
      c00 = _mm256_add_ps(c00, avx_noise_estimate);
      c11 = _mm256_add_ps(c11, avx_noise_estimate);

      /* 5. detC */
      __m256 detC = srslte_mat_2x2_det_avx(c00, c01, c10, c11);
      __m256 inv_detC = srslte_mat_cf_recip_avx(detC);
      inv_detC = _mm256_mul_ps(avx_noise_estimate, inv_detC);

      __m256 den0 = _MM256_PROD_PS(c00, inv_detC);
      __m256 den1 = _MM256_PROD_PS(c11, inv_detC);

      __m256 gamma0 = _mm256_sub_ps(_mm256_rcp_ps(den0), avx_ones);
      __m256 gamma1 = _mm256_sub_ps(_mm256_rcp_ps(den1), avx_ones);

      /* Add for averaging */
      __m256 sinr_avx = _mm256_permute_ps(_mm256_add_ps(gamma0, gamma1), 0b00101000);
      __attribute__((aligned(256))) float sinr[8];
      _mm256_store_ps(sinr, sinr_avx);

      sinr_list[i] += sinr[0] + sinr[2] + sinr[4] + sinr[6];

      count += 4;
    }

    /* Divide average by noise */
    if (count) {
      sinr_list[i] /= count;
    }

    if (sinr_list[i] > max_sinr) {
      max_sinr = sinr_list[i];
      *pmi = i;
    }
  }

  return i;
}

#endif /* LV_HAVE_AVX */

/* PMI Select for 2 layers */
int srslte_precoding_pmi_select_2l(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                   float noise_estimate, uint32_t *pmi,
                                   float sinr_list[SRSLTE_MAX_CODEBOOKS]) {

  int ret;
#ifdef LV_HAVE_AVX
  ret = srslte_precoding_pmi_select_2l_avx(h, nof_symbols, noise_estimate, pmi, sinr_list);
#else
  #ifdef LV_HAVE_SSE
  ret = srslte_precoding_pmi_select_2l_sse(h, nof_symbols, noise_estimate, pmi, sinr_list);
#else
  ret = srslte_precoding_pmi_select_2l_gen(h, nof_symbols, noise_estimate, pmi, sinr_list);
#endif /* LV_HAVE_SSE */
#endif /* LV_HAVE_AVX */

  INFO("Precoder PMI Select for 2 layers SINR=[%.1fdB; %.1fdB] PMI=%d\n", 10 * log10(sinr_list[0]),
       10 * log10(sinr_list[1]), *pmi);

  return ret;
}

int srslte_precoding_pmi_select(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols,
                                float noise_estimate, int nof_layers, uint32_t *pmi,
                                float sinr[SRSLTE_MAX_CODEBOOKS]) {
  int ret;

  if (sinr == NULL || pmi == NULL) {
    ERROR("Null pointer");
    ret = SRSLTE_ERROR_INVALID_INPUTS;
  } else if (nof_layers == 1) {
    ret = srslte_precoding_pmi_select_1l(h, nof_symbols, noise_estimate, pmi, sinr);
  } else if (nof_layers == 2) {
    ret = srslte_precoding_pmi_select_2l(h, nof_symbols, noise_estimate, pmi, sinr);
  } else {
    ERROR("Wrong number of layers");
    ret = SRSLTE_ERROR_INVALID_INPUTS;
  }

  return ret;
}

/* PMI Select for 1 layer */
float srslte_precoding_2x2_cn_gen(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_symbols) {
  uint32_t count = 0;
  float cn_avg = 0.0f;

  for (uint32_t i = 0; i < nof_symbols; i += PMI_SEL_PRECISION) {
    /* 0. Load channel matrix */
    cf_t h00 = h[0][0][i];
    cf_t h01 = h[1][0][i];
    cf_t h10 = h[0][1][i];
    cf_t h11 = h[1][1][i];

    cn_avg += srslte_mat_2x2_cn(h00, h01, h10, h11);

    count++;
  }

  if (count) {
    cn_avg /= count;
  }

  return cn_avg;
}

/* Computes the condition number for a given number of antennas,
 * stores in the parameter *cn the Condition Number in dB */
int srslte_precoding_cn(cf_t *h[SRSLTE_MAX_PORTS][SRSLTE_MAX_PORTS], uint32_t nof_tx_antennas,
                          uint32_t nof_rx_antennas, uint32_t nof_symbols, float *cn) {
  if (nof_tx_antennas == 2 && nof_rx_antennas == 2) {
    *cn = srslte_precoding_2x2_cn_gen(h, nof_symbols);
    return SRSLTE_SUCCESS;
  } else {
    DEBUG("MIMO Condition Number calculation not implemented for %d??%d", nof_tx_antennas, nof_rx_antennas);
    return SRSLTE_ERROR;
  }
}

