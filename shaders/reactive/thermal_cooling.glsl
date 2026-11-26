#ifndef FR_THERMAL_COOLING_GLSL
#define FR_THERMAL_COOLING_GLSL

vec2 thermal_storm_heat(vec3 energy) {
    const float retained_fraction = 0.3;
    const float fraction_residual = -1.1920928955078125e-8;
    precise float main = energy.x * retained_fraction;
    precise float error = fma(energy.x, retained_fraction, -main);
    precise float tail = fma(energy.x, fraction_residual, error) +
        (energy.y + energy.z) * retained_fraction;
    precise float heat = main + tail;
    precise float remainder = (main - heat) + tail;
    return vec2(heat, remainder);
}

#endif
