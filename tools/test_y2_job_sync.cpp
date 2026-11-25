// Exhaustive two-page combinations and randomized larger page ranges.
#include "../src/burn/drv/sega/y2_job_sync.h"
#include <stdio.h>
#include <stdlib.h>
using namespace Y2JobSync;

static void Check(const uint8_t *p, const uint8_t *r, unsigned first, unsigned last, bool write)
{
	bool pc = false, rc = false, old = false;
	for (unsigned i=first; i<=last; i++) {
		pc |= write ? p[i] != 0 : p[i] == 2;
		rc |= write ? r[i] != 0 : r[i] == 2;
		unsigned combined = p[i] > r[i] ? p[i] : r[i];
		old |= write ? combined != 0 : combined == 2;
	}
	Action a = Plan(p,r,first,last,write);
	if ((a != None) != old || a != (pc ? DrainAll : rc ? WaitRunning : None)) exit(1);
}

int main()
{
	uint8_t p[64] = {}, r[64] = {};
	for (unsigned i=0; i<81; i++) {
		unsigned n=i;
		p[0]=n%3; n/=3; p[1]=n%3; n/=3; r[0]=n%3; n/=3; r[1]=n%3;
		for (unsigned w=0; w<2; w++) for(unsigned f=0; f<2; f++)
			for(unsigned l=f; l<2; l++) Check(p,r,f,l,w);
	}
	uint32_t seed=0xb3d72109;
	for (unsigned c=0; c<100000; c++) {
		for (unsigned i=0; i<64; i++) {
			seed=seed*1664525u+1013904223u; p[i]=(seed>>16)%3;
			seed=seed*1664525u+1013904223u; r[i]=(seed>>16)%3;
		}
		unsigned f=c%64,l=f+(c/64)%(64-f);
		Check(p,r,f,l,c&1);
	}
	puts("Y2 page dependency planner: PASS");
}
