#ifndef FR_THERMAL_COOLING_GLSL
#define FR_THERMAL_COOLING_GLSL

vec2 thermal_storm_heat(vec3 energy) {
    const float retained_fraction = 0.3;
    /* Difference between 3/10 and its nearest float. */
    const float fraction_residual = -1.1920928955078125e-8;
    precise float leading = energy.x * retained_fraction;
    precise float product_residual = fma(energy.x, retained_fraction, -leading);
    precise float tail = fma(energy.x, fraction_residual, product_residual) +
        (energy.y + energy.z) * retained_fraction;
    precise float heat = leading + tail;
    precise float remainder = (leading - heat) + tail;
    return vec2(heat, remainder);
}

#endif
