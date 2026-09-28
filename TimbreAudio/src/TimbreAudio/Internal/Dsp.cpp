#include "Dsp.h"

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#define TIMBRE_HAS_SSE 1
#endif

namespace Timbre::Internal
{
	DenormalGuard::DenormalGuard()
	{
#ifdef TIMBRE_HAS_SSE
		m_State = _mm_getcsr();
		_mm_setcsr(m_State | 0x8040); // flush to zero, denormals are zero
#endif
	}

	DenormalGuard::~DenormalGuard()
	{
#ifdef TIMBRE_HAS_SSE
		_mm_setcsr(m_State);
#endif
	}
}
