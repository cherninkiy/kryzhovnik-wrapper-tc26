#ifndef PARAMS_H
#define PARAMS_H


#include "config.h"
#include <stdint.h>

#if defined(__has_include)
#if __has_include("current-params.h")
#include "current-params.h"
#else
#define KRYZHOVNIK_PARAMSET_NAME "medium"
#include "paramsets/params_medium.h"
#endif
#else
#define KRYZHOVNIK_PARAMSET_NAME "medium"
#include "paramsets/params_medium.h"
#endif

#define PQS_n 256
#define PQS_p (1 << PQS_mu)
#define PQS_q (1 << PQS_nu)

#ifndef PQS_s
#error "PQS_s must be defined by selected kryzhovnik paramset"
#endif

#define CRHBYTES 48
#define SEEDBYTES 32
#define POLW1_SIZE_PACKED ((PQS_n*4)/8)

// A polynomial in R_q, represented by a vector of its coefficients (x^0, x^1, ..., x^(PQS_n-1))
typedef struct{
  uint32_t coeffs[PQS_n];
} poly;

// A vector of length PQS_l of polynomials
typedef struct{
	poly polynomial[PQS_l];
} polyvecl;

// A vector of length PQS_k of polynomials
typedef struct{
	poly polynomial[PQS_k];
} polyveck;

// Rotation matrix of a polynomial
typedef struct{
  poly row[PQS_n];
} polyrot;

// Matrix of size PQS_k*PQS_l of polynomials
typedef struct{
  polyrot elem[PQS_k][PQS_l];
} polymatkl;

// Secret key
typedef struct{
	polyvecl s;
} sk_t;

// Public key
typedef struct{
	unsigned char seedA[SEEDBYTES];
	polyveck t;
} vk_t;

// Signature
typedef struct{
	poly c;
	polyvecl z;
} signat_t;

#endif

