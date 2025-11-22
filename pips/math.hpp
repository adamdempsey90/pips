#ifndef PIPS_MATH_HPP_
#define PIPS_MATH_HPP_

#include <cmath>
#include <limits>

#include "types.hpp"

namespace pips {

// These are special implementations to return exactly 0 or 1 for special values
inline Real sin(Real x) {
    const Real pi = std::acos(-1.0L);
    const Real epsilon = std::numeric_limits<Real>::epsilon() * 100.0L;
    
    Real normalized = std::fmod(x, 2.0L * pi);
    if (normalized > pi) normalized -= 2.0L * pi;
    if (normalized < -pi) normalized += 2.0L * pi;
    
    if (std::abs(normalized) < epsilon || std::abs(std::abs(normalized) - pi) < epsilon) {
        return 0.0L;
    }
    
    if (std::abs(normalized - pi/2.0L) < epsilon) {
        return 1.0L;
    }
    
    if (std::abs(normalized + pi/2.0L) < epsilon) {
        return -1.0L;
    }
    
    return std::sin(x);
}

inline Real cos(Real x) {
    const Real pi = std::acos(-1.0L);
    const Real epsilon = std::numeric_limits<Real>::epsilon() * 100.0L;
    
    Real normalized = std::fmod(x, 2.0L * pi);
    if (normalized > pi) normalized -= 2.0L * pi;
    if (normalized < -pi) normalized += 2.0L * pi;
    
    if (std::abs(normalized) < epsilon) {
        return 1.0L;
    }
    if (std::abs(std::abs(normalized) - pi) < epsilon) {
        return -1.0L;
    }
    if (std::abs(std::abs(normalized) - pi/2.0L) < epsilon) {
        return 0.0L;
    }
    
    return std::cos(x);
}

inline Real tan(Real x) {
    const Real pi = std::acos(-1.0L);
    const Real epsilon = std::numeric_limits<Real>::epsilon() * 100.0L;
    
    Real normalized = std::fmod(x, pi);
    if (normalized >= pi/2.0L) normalized -= pi;
    if (normalized < -pi/2.0L) normalized += pi;
    
    if (std::abs(normalized) < epsilon) {
        return 0.0L;
    }
    
    return std::tan(x);
}

} // namespace pips

#endif // PIPS_MATH_HPP_