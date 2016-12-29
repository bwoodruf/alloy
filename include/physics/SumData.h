#ifndef PHYS_SUMDATA_H
#define PHYS_SUMDATA_H
#include <xmmintrin.h>
#include "AlloyMath.h"
namespace aly {
	// The actual elements of data that must be summmed. These elements will be
	//  used to store different things - v will be mixi
	//  during the shape matching, and (cr - Rrcr) during goal position calculation;
	//  M will be EpmiT during shape matching but Rr
	//  during goal position calculation.
	//(align(16)) 
	struct SumData
	{
	public:
		struct {
			float3 v;
			float3x3 M;
		};
		struct {
			__m128 m1, m2, m3;
		};
	};
}
#endif
