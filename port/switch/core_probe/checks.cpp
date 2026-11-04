#include <cmath>
#include <libvu0.h>

#include "mg_math.hpp"

static_assert(sizeof(void *) == 8);
static_assert(sizeof(s8) == 1 && static_cast<s8>(-1) < 0);

extern "C" int ctSwitchMathChecks() {
    int failures = 0;
    float input[4] = {3.0f, 4.0f, 0.0f, 1.0f};
    float normal[4] = {};
    mgNormalizeVector(normal, input, 1.0f);
    failures += std::fabs(normal[0] - 0.6f) > 0.0001f;
    failures += std::fabs(normal[1] - 0.8f) > 0.0001f;

    float matrix[4][4];
    float translation[4] = {10.0f, 20.0f, 30.0f, 1.0f};
    float transformed[4] = {};
    sceVu0UnitMatrix(matrix);
    sceVu0TransMatrix(matrix, matrix, translation);
    sceVu0ApplyMatrix(transformed, matrix, input);
    failures += std::fabs(transformed[0] - 13.0f) > 0.0001f;
    failures += std::fabs(transformed[1] - 24.0f) > 0.0001f;
    failures += std::fabs(transformed[2] - 30.0f) > 0.0001f;

    // This implementation comes from the generated, override-stripped PS2 unit.
    float from[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float to[4] = {4.0f, 8.0f, 12.0f, 1.0f};
    float interpolated[4] = {};
    mgVectorInterpolate(interpolated, from, to, 2.0f, MG_INTERPOLATE_FRACTION);
    failures += std::fabs(interpolated[0] - 2.0f) > 0.0001f;
    failures += std::fabs(interpolated[1] - 4.0f) > 0.0001f;
    failures += std::fabs(interpolated[2] - 6.0f) > 0.0001f;
    return failures;
}
