#pragma once
#include <cmath>

/**
 * 1st-Order Antiderivative Antialiasing (ADAA) for tanh saturation.
 * 
 * This eliminates aliasing artifacts caused by the nonlinear waveshaping
 * by integrating the waveshaper and computing the difference quotient.
 * 
 * Math:
 *   f(x) = tanh(x)
 *   AD1(x) = log(cosh(x))  -- the antiderivative
 *   y[n] = (AD1(x[n]) - AD1(x[n-1])) / (x[n] - x[n-1])
 * 
 * Stability:
 *   - For |x| > 10: AD1(x) ≈ |x| - log(2) to avoid overflow
 *   - For |x[n] - x[n-1]| < epsilon: fallback to tanh(midpoint)
 */
class ADAAProcessor {
public:
    ADAAProcessor() = default;

    void reset() {
        x1 = 0.0f;
        ad1_x1 = 0.0f;
    }

    /**
     * Process a single sample through ADAA tanh saturation.
     * @param x Current input sample (pre-saturated, already scaled by drive)
     * @return Anti-aliased saturated output
     */
    float processSample(float x) {
        float ad1_x = antiderivative(x);
        float y;

        float delta = x - x1;
        if (std::abs(delta) < epsilon) {
            // Near-zero denominator: use midpoint fallback
            y = std::tanh((x + x1) * 0.5f);
        } else {
            // Standard ADAA formula
            y = (ad1_x - ad1_x1) / delta;
        }

        // Update state for next sample
        x1 = x;
        ad1_x1 = ad1_x;

        return y;
    }

private:
    /**
     * First antiderivative of tanh(x) = log(cosh(x))
     * With numerical stability for large |x|.
     */
    static float antiderivative(float x) {
        constexpr float threshold = 10.0f;
        constexpr float log2 = 0.6931471805599453f;  // log(2)

        if (x > threshold) {
            // For large positive x: log(cosh(x)) ≈ x - log(2)
            return x - log2;
        } else if (x < -threshold) {
            // For large negative x: log(cosh(x)) ≈ -x - log(2) = |x| - log(2)
            return -x - log2;
        } else {
            // Standard computation for moderate x
            return std::log(std::cosh(x));
        }
    }

    float x1 = 0.0f;          // Previous input sample
    float ad1_x1 = 0.0f;      // Antiderivative at previous input
    static constexpr float epsilon = 1.0e-5f;
};
