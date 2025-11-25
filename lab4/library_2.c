#include <math.h>
#include "library.h"

// NOTE: MSVC compiler does not export symbols unless annotated
#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT float cos_derivative(float a, float dx) {
	if (dx == 0.0f) {
		return NAN;
	}
	return (cosf(a + dx) - cosf(a - dx)) / (2.0f * dx);
}
EXPORT float area(float a, float b) {
	return 0.5f * a * b;
}