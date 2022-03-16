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

#include "srslte/phy/resampling/resample_arb.h"
#include "srslte/phy/utils/debug.h"
#include "srslte/phy/utils/vector.h"
#include <math.h>
#include <string.h>

// clang-format off
float srslte_resample_arb_polyfilt[SRSLTE_RESAMPLE_ARB_N][SRSLTE_RESAMPLE_ARB_M] __attribute__((aligned(256))) =
{{0.000499262532685, 0.000859897001646, -0.008521087467670, 0.994530856609344, 0.017910413444042, -0.006922415923327, 0.002400347497314, 0.000000000000000 },
{-0.001216900418513, 0.008048813790083, -0.032752435654402, 0.991047739982605, 0.046474494040012, -0.015253192745149, 0.004479591734707, -0.001903604716063 },
{-0.002745266072452, 0.014606527984142, -0.054737392812967, 0.984105646610260, 0.077085755765438, -0.024075405672193, 0.006728826556355, -0.001750442315824 },
{-0.004077596124262, 0.020508514717221, -0.074447125196457, 0.973753631114960, 0.109637774527073, -0.033325243741274, 0.009130494669080, -0.001807302702218 },
{-0.005218582693487, 0.025736490264535, -0.091872826218605, 0.960064172744751, 0.144006818532944, -0.042925916612148, 0.011667170561850, -0.002005048329011 },
{-0.006178495008498, 0.030282959342003, -0.107022657990456, 0.943133354187012, 0.180049657821655, -0.052793655544519, 0.014315498992801, -0.002303606597707 },
{-0.006951034534723, 0.034145597368479, -0.119924493134022, 0.923079371452332, 0.217606633901596, -0.062832623720169, 0.017053086310625, -0.002673825016245 },
{-0.007554128300399, 0.037334490567446, -0.130620718002319, 0.900041282176971, 0.256499618291855, -0.072941005229950, 0.019850222393870, -0.003099294845015 },
{-0.007992693223059, 0.039863564074039, -0.139171004295349, 0.874178349971771, 0.296536892652512, -0.083006605505943, 0.022677028551698, -0.003564415033907 },
{-0.008278168737888, 0.041754297912121, -0.145647943019867, 0.845668911933899, 0.337510257959366, -0.092912115156651, 0.025496333837509, -0.004060222767293 },
{-0.008423951454461, 0.043032847344875, -0.150139778852463, 0.814707279205322, 0.379200965166092, -0.102530807256699, 0.028271337971091, -0.004574770107865 },
{-0.008441128768027, 0.043734934180975, -0.152744457125664, 0.781503617763519, 0.421377241611481, -0.111732520163059, 0.030959306284785, -0.005100453738123 },
{-0.008345136418939, 0.043892618268728, -0.153572276234627, 0.746281027793884, 0.463798433542252, -0.120380215346813, 0.033518772572279, -0.005625340621918 },
{-0.008147838525474, 0.043551109731197, -0.152741402387619, 0.709274411201477, 0.506216108798981, -0.128335043787956, 0.035902760922909, -0.006140888202935 },
{-0.007865131832659, 0.042750217020512, -0.150379851460457, 0.670727193355560, 0.548376381397247, -0.135454908013344, 0.038066454231739, -0.006634012795985 },
{-0.007508971262723, 0.041538298130035, -0.146618664264679, 0.630890727043152, 0.590020775794983, -0.141596958041191, 0.039960436522961, -0.007094909437001 },
{-0.007094909437001, 0.039960436522961, -0.141596958041191, 0.590020775794983, 0.630890727043152, -0.146618664264679, 0.041538298130035, -0.007508971262723 },
{-0.006634012795985, 0.038066454231739, -0.135454908013344, 0.548376381397247, 0.670727193355560, -0.150379851460457, 0.042750217020512, -0.007865131832659 },
{-0.006140888202935, 0.035902760922909, -0.128335043787956, 0.506216108798981, 0.709274411201477, -0.152741402387619, 0.043551109731197, -0.008147838525474 },
{-0.005625340621918, 0.033518772572279, -0.120380215346813, 0.463798433542252, 0.746281027793884, -0.153572276234627, 0.043892618268728, -0.008345136418939 },
{-0.005100453738123, 0.030959306284785, -0.111732520163059, 0.421377241611481, 0.781503617763519, -0.152744457125664, 0.043734934180975, -0.008441128768027 },
{-0.004574770107865, 0.028271337971091, -0.102530807256699, 0.379200965166092, 0.814707279205322, -0.150139778852463, 0.043032847344875, -0.008423951454461 },
{-0.004060222767293, 0.025496333837509, -0.092912115156651, 0.337510257959366, 0.845668911933899, -0.145647943019867, 0.041754297912121, -0.008278168737888 },
{-0.003564415033907, 0.022677028551698, -0.083006605505943, 0.296536892652512, 0.874178349971771, -0.139171004295349, 0.039863564074039, -0.007992693223059 },
{-0.003099294845015, 0.019850222393870, -0.072941005229950, 0.256499618291855, 0.900041282176971, -0.130620718002319, 0.037334490567446, -0.007554128300399 },
{-0.002673825016245, 0.017053086310625, -0.062832623720169, 0.217606633901596, 0.923079371452332, -0.119924493134022, 0.034145597368479, -0.006951034534723 },
{-0.002303606597707, 0.014315498992801, -0.052793655544519, 0.180049657821655, 0.943133354187012, -0.107022657990456, 0.030282959342003, -0.006178495008498 },
{-0.002005048329011, 0.011667170561850, -0.042925916612148, 0.144006818532944, 0.960064172744751, -0.091872826218605, 0.025736490264535, -0.005218582693487 },
{-0.001807302702218, 0.009130494669080, -0.033325243741274, 0.109637774527073, 0.973753631114960, -0.074447125196457, 0.020508514717221, -0.004077596124262 },
{-0.001750442315824, 0.006728826556355, -0.024075405672193, 0.077085755765438, 0.984105646610260, -0.054737392812967, 0.014606527984142, -0.002745266072452 },
{-0.001903604716063, 0.004479591734707, -0.015253192745149, 0.046474494040012, 0.991047739982605, -0.032752435654402, 0.008048813790083, -0.001216900418513 },
{0.000000000000000, 0.002400347497314, -0.006922415923327, 0.017910413444042, 0.994530856609344, -0.008521087467670, 0.000859897001646, 0.000499262532685 }};




 float srslte_resample_arb_polyfilt_35[SRSLTE_RESAMPLE_ARB_N_35][SRSLTE_RESAMPLE_ARB_M] __attribute__((aligned(256))) =
 {{0.000002955485215,  0.000657994314549,  -0.033395686652146,   0.188481383863832,  0.704261032406613,   0.171322660416961,  -0.032053439082436,   0.000722236729272},
{0.000003596427925,   0.000568080243211,  -0.034615802155152,   0.206204344739138,   0.702921418438421,   0.154765509342932,  -0.030612377229395,   0.000764085430796},
{0.000005121937258,  0.000449039680445,  -0.035689076986744,   0.224449928603191,  0.700248311996698,   0.138842912406449,  -0.029094366813032,   0.000786624971348},
{0.000007718609465,   0.000297261794949,  -0.036589488825594,   0.243172347408947,   0.696253915778072,   0.123583456372980,  -0.027519775698206,   0.000792734165095},
{0.000011575047600,   0.000109005258838,  -0.037289794881918,   0.262321777662990,   0.690956433427623,   0.109011322456059,  -0.025907443019580,   0.000785077624893},
{0.000016882508098,  -0.000119571475197,  -0.037761641428480,   0.281844530653151,   0.684379949985066,   0.095146306185631,  -0.024274662580170,   0.000766100891437},
{0.000023834849457,  -0.000392374989948,  -0.037975689603840,   0.301683254255907,   0.676554274013489,   0.082003866618022,  -0.022637179782782,   0.000738028846774},
{0.000032627688404,  -0.000713336731203,  -0.037901757319987,   0.321777165554580,   0.667514742706306,   0.069595203590358,  -0.021009201268519,   0.000702867092175},
{0.000043456678971,  -0.001086367817537,  -0.037508976943232,   0.342062313218325,   0.657301991568526,   0.057927361522269,  -0.019403416364306,   0.000662405963124},
{0.000056514841640,  -0.001515308860252,  -0.036765968249551,   0.362471868314761,   0.645961690553484,   0.047003358086968,  -0.017831029382043,   0.000618226851332},
{0.000071988882997,  -0.002003874882511,  -0.035641025985104,   0.382936441958648,   0.633544248803284,   0.036822335913581,  -0.016301801765629,   0.000571710504985},
{0.000090054461223,  -0.002555595491550,  -0.034102321191048,   0.403384427938144,   0.620104490387788,   0.027379735343965,  -0.014824103048525,   0.000524046983526},
{0.000110870369116,  -0.003173750525766,  -0.032118115280935,   0.423742368211529,   0.605701303660781,   0.018667486151043,  -0.013404969563367,   0.000476246951838},
{0.000134571624077,  -0.003861301468941,  -0.029656985690720,   0.443935338933583,   0.590397267050922,   0.010674216032341,  -0.012050169836157,   0.000429154010367},
{0.000161261473605,  -0.004620818996097,  -0.026688061757620,   0.463887354454611,   0.574258254277514,   0.003385473622292,  -0.010764275599988,   0.000383457772129},
{0.000191002345063,  -0.005454407088752,  -0.023181269326746,   0.483521786538780,   0.557353022125450,  -0.003216036280022,  -0.009550737376604,   0.000339707414324},
{0.000223805789820,  -0.006363624230896,  -0.019107582435436,   0.502761795874207,   0.539752784028766,  -0.009150207594916,  -0.008411963597610,   0.000298325451050},
{0.000259621494011,  -0.007349402269867,  -0.014439280286493,   0.521530772797177,   0.521530772797177,  -0.014439280286493,  -0.007349402269867,   0.000259621494011},
{0.000298325451050,  -0.008411963597610,  -0.009150207594916,   0.539752784028765,   0.502761795874207,  -0.019107582435436,  -0.006363624230896,   0.000223805789820},
{0.000339707414324,  -0.009550737376604,  -0.003216036280022,   0.557353022125450,   0.483521786538780,  -0.023181269326746,  -0.005454407088752,   0.000191002345063},
{0.000383457772129,  -0.010764275599988,   0.003385473622292,   0.574258254277514,   0.463887354454611,  -0.026688061757620,  -0.004620818996097,   0.000161261473605},
{0.000429154010367,  -0.012050169836157,   0.010674216032341,   0.590397267050922,   0.443935338933583,  -0.029656985690720,  -0.003861301468941,   0.000134571624077},
{0.000476246951838,  -0.013404969563367,   0.018667486151043,   0.605701303660781,   0.423742368211529,  -0.032118115280935,  -0.003173750525766,   0.000110870369116},
{0.000524046983526,  -0.014824103048525,   0.027379735343965,   0.620104490387787,   0.403384427938144,  -0.034102321191048,  -0.002555595491550,   0.000090054461223},
{0.000571710504985,  -0.016301801765629,   0.036822335913581,   0.633544248803283,   0.382936441958648,  -0.035641025985104,  -0.002003874882511,   0.000071988882997},
{0.000618226851332,  -0.017831029382043,   0.047003358086968,   0.645961690553484,   0.362471868314761,  -0.036765968249551,  -0.001515308860252,   0.000056514841640},
{0.000662405963124,  -0.019403416364306,   0.057927361522269,   0.657301991568526,   0.342062313218325,  -0.037508976943232,  -0.001086367817537,   0.000043456678971},
{0.000702867092175,  -0.021009201268519,   0.069595203590358,   0.667514742706306,   0.321777165554580,  -0.037901757319987,  -0.000713336731203,   0.000032627688404},
{0.000738028846774,  -0.022637179782782,   0.082003866618022,   0.676554274013489,   0.301683254255907,  -0.037975689603840,  -0.000392374989948,   0.000023834849457},
{0.000766100891437,  -0.024274662580170,   0.095146306185631,   0.684379949985066,   0.281844530653151,  -0.037761641428480,  -0.000119571475197,   0.000016882508098},
{0.000785077624893,  -0.025907443019580,   0.109011322456059,   0.690956433427623,   0.262321777662990,  -0.037289794881918,   0.000109005258838,   0.000011575047600},
{0.000792734165095,  -0.027519775698206,   0.123583456372980,   0.696253915778072,   0.243172347408947,  -0.036589488825594,   0.000297261794949,   0.000007718609465},
{0.000786624971348,  -0.029094366813032,   0.138842912406449,   0.700248311996698,   0.224449928603191,  -0.035689076986744,   0.000449039680445,   0.000005121937258},
{0.000764085430796,  -0.030612377229395,   0.154765509342932,   0.702921418438421,   0.206204344739138,  -0.034615802155152,   0.000568080243211,   0.000003596427925},
{0.000722236729272,  -0.032053439082436,   0.171322660416961,   0.704261032406613,   0.188481383863832,  -0.033395686652146,   0.000657994314549 ,  0.000002955485215}};

// clang-format on
static inline cf_t srslte_resample_arb_dot_prod(cf_t* x, float* y, int len)
{
  cf_t res1 = srslte_vec_dot_prod_cfc(x, y, len);
  return res1;
}

// Right-shift our window of samples
void srslte_resample_arb_push(srslte_resample_arb_t* q, cf_t x)
{

  memmove(&q->reg[1], &q->reg[0], (SRSLTE_RESAMPLE_ARB_M - 1) * sizeof(cf_t));
  q->reg[0] = x;
}

// Initialize our struct
void srslte_resample_arb_init(srslte_resample_arb_t* q, float rate, bool interpolate)
{

  memset(q->reg, 0, SRSLTE_RESAMPLE_ARB_M * sizeof(cf_t));
  q->acc         = 0.0;
  q->rate        = rate;
  q->interpolate = interpolate;
  q->step        = (1 / rate) * SRSLTE_RESAMPLE_ARB_N;
}

// Resample a block of input data
int srslte_resample_arb_compute(srslte_resample_arb_t* q, cf_t* input, cf_t* output, int n_in)
{
  int   cnt   = 0;
  int   n_out = 0;
  int   idx   = 0;
  cf_t  res1, res2;
  cf_t* filter_input;
  float frac = 0;
  memset(q->reg, 0, SRSLTE_RESAMPLE_ARB_M * sizeof(cf_t));

  while (cnt < n_in) {

    if (cnt < SRSLTE_RESAMPLE_ARB_M) {
      memcpy(&q->reg[SRSLTE_RESAMPLE_ARB_M - cnt], input, (cnt) * sizeof(cf_t));
      filter_input = q->reg;
    } else {
      filter_input = &input[cnt - SRSLTE_RESAMPLE_ARB_M];
    }

    res1 = srslte_resample_arb_dot_prod(filter_input, srslte_resample_arb_polyfilt[idx], SRSLTE_RESAMPLE_ARB_M);
    if (q->interpolate) {
      res2 = srslte_resample_arb_dot_prod(
          filter_input, srslte_resample_arb_polyfilt[(idx + 1) % SRSLTE_RESAMPLE_ARB_N], SRSLTE_RESAMPLE_ARB_M);
    }

    if (idx == SRSLTE_RESAMPLE_ARB_N) {
      *output = res1;
    } else {
      *output = (q->interpolate) ? (res1 + (res2 - res1) * frac) : res1;
    }

    output++;
    n_out++;
    q->acc += q->step;
    idx = (int)(q->acc);

    while (idx >= SRSLTE_RESAMPLE_ARB_N) {
      q->acc -= SRSLTE_RESAMPLE_ARB_N;
      idx -= SRSLTE_RESAMPLE_ARB_N;
      if (cnt < n_in) {
        cnt++;
      }
    }

    if (q->interpolate) {
      frac = q->acc - idx;
      if (frac < 0)
        frac = frac * (-1);
    }
  }
  return n_out;
}
