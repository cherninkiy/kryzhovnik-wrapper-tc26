#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "params.h"
#include "poly_mul.h"
#include "cpucycles.h"
#include "randombytes.h"
#include "sign.h"
#define MSECS(t) ((double)(t)/(2600000))

unsigned long long timing_overhead;

#if TEST_MODE == 1
int main(){	
	double tm;
	
	tm = getCPUTime( );
	for(int loop=0; loop<NTESTS; ++loop){
		
		vk_t vk; // Public key
		sk_t sk; // Secret key
		
		printf("Generating keypair ... ");
		tm = getCPUTime( );
		PQS_keygen(&vk, &sk);
		printf("Ok\n");
		printf("Keypair is generated in %f sec.\n", getCPUTime() - tm);
		
		
		// A message to be signed
		// ToDo: Add input from a file
		unsigned char m[] = "My test message";
		
		signat_t sig; // Signature
		tm = getCPUTime( );
		PQS_sign(&sig, &m[0], sizeof(m), &sk, &vk);
		printf("Signed Ok\n");
		printf("Signing is completed in %f sec.\n", getCPUTime() - tm);
		
		
		tm = getCPUTime( );
		bool success = PQS_verify(&sig, &m[0], sizeof(m), &vk);
		if(success){
			printf("Verify Ok\n");
		} else {
			printf("Verify fail\n");
			return 0;
		}
		printf("Verification is completed in %f sec.\n", getCPUTime() - tm);
	}
	
	return 0;
	
}
#elif TEST_MODE == 2
int main(){	
	double tm;
	
	tm = getCPUTime( );
	for(int loop=0; loop<NTESTS; ++loop){
		
		vk_t vk; // Public key
		sk_t sk; // Secret key
		
		PQS_keygen(&vk, &sk);
		
		// A message to be signed
		// ToDo: Add input from a file
		unsigned char m[] = "My test message";
		
		signat_t sig; // Signature
		PQS_sign(&sig, &m[0], sizeof(m), &sk, &vk);
		
		bool success = PQS_verify(&sig, &m[0], sizeof(m), &vk);
		if(!success){
			printf("Verify fail\n");
			return 0;
		}

		if(loop % 1000 == 0){
			printf("%dk tests ok in %f\n", loop / 1000, getCPUTime() - tm);
			tm = getCPUTime( );
		}
	}
	
	return 0;
	
}

#elif TEST_MODE == 3
static int cmp_llu(const void *a, const void *b) {
  if(*(unsigned long long *)a < *(unsigned long long *)b) return -1;
  if(*(unsigned long long *)a > *(unsigned long long *)b) return 1;
  return 0;
}

static unsigned long long median(unsigned long long *l, size_t llen) {
  qsort(l,llen,sizeof(unsigned long long),cmp_llu);

  if(llen%2) return l[llen/2];
  else return (l[llen/2-1]+l[llen/2])/2;
}

static unsigned long long average(unsigned long long *t, size_t tlen) {
  unsigned long long acc=0;
  size_t i;

  for(i=0;i<tlen;i++)
    acc += t[i];

  return acc/(tlen);
}

void print_results(const char *s, unsigned long long *t, size_t tlen) {
  unsigned long long tmp;

  printf("%s\n", s);

  tmp = median(t, tlen);
  printf("median: %llu ticks @ 2.6 GHz (%.4g msecs)\n", tmp, MSECS(tmp));

  tmp = average(t, tlen);
  printf("average: %llu ticks @ 2.6 GHz (%.4g msecs)\n", tmp, MSECS(tmp));

  printf("\n");
}


int main(){	
	unsigned long long tkeygen[NTESTS], tsign[NTESTS], tverify[NTESTS];
	unsigned char m[MLEN_TEST];
	unsigned int i;
	//unsigned long long j;
	bool ret;
	vk_t vk; // Public key
	sk_t sk; // Secret key
	signat_t sig; // Signature
	timing_overhead = cpucycles_overhead();

	for(i=0; i<NTESTS; ++i){
		kryzhovnik_randombytes(m, MLEN_TEST);
		tkeygen[i] = cpucycles_start();
		PQS_keygen(&vk, &sk);
		tkeygen[i] = cpucycles_stop() - tkeygen[i] - timing_overhead;
		
		tsign[i] = cpucycles_start();
		PQS_sign(&sig, &m[0], sizeof(m), &sk, &vk);
		tsign[i] = cpucycles_stop() - tsign[i] - timing_overhead;
		//printf("%llu \n", tsign[i]);
		
		tverify[i] = cpucycles_start();
		ret = PQS_verify(&sig, &m[0], sizeof(m), &vk);
		tverify[i] = cpucycles_stop() - tverify[i] - timing_overhead;
		
		if(!ret) {
			printf("Verification failed\n");
			return -1;
		}
		
	}
  print_results("keygen:", tkeygen, NTESTS);
  print_results("sign: ", tsign, NTESTS);
  print_results("verify: ", tverify, NTESTS);
	
	return 0;
	
}
#endif
