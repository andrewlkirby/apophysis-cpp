#pragma once

#include <cmath>

#include "DeviceRng.cuh"
#include "DeviceTypes.h"
#include "VariationKinds.h"

// Device ports of Variation::calc() bodies - see XForm.cpp (the 29 local
// variations) and src/core/variations/Var*.cpp (the registered ones listed
// in VariationKinds.cpp). Each is translated close to verbatim from its
// CPU source; deviations from the CPU math are called out per-function.
//
// RNG simplification (applies to the 4 local variations that use a
// "gaussian-ish" blur - gaussian_blur/zblur/blur3D/pre_blur): the CPU side
// keeps a small per-xform 4-slot ring buffer (gaussRnd_/gaussN_, see
// XForm.h) that's refilled one slot at a time so each call reuses 3 old
// draws + 1 fresh one. Carrying that same ring buffer through the GPU's
// per-thread persistent state is extra bookkeeping for no visible benefit -
// both this version and the CPU's amortized version are just "sum of 4
// uniform01() draws, minus 2" (a cheap CLT approximation of a standard
// normal), so this draws all 4 fresh every call instead of reusing 3.
// Consumes more of the RNG stream per call than the CPU path, and (like
// every other GPU-vs-CPU RNG difference - see Renderer.h's own note that
// even CPU multithreaded rendering isn't bit-reproducible) is validated by
// PSNR similarity, never bit-equality - see docs/GPU_RENDERING_PLAN.md.

namespace apo::gpu {

constexpr double kDeviceEps = 1e-300; // matches XForm.cpp's kEps
constexpr double kDevicePi = 3.14159265358979323846;

// Per-point, per-variation-step context: mirrors the (tx,ty,tz)->(px,py,pz)
// accumulation contract every CPU Variation::calc()/XForm local-variation
// method follows (see Variation.h's own doc comment), plus the handful of
// owning-xform fields (a..f, post-transform coefs) some variations read.
// `params` points at this op's slice of the DeviceFlame-wide params array
// (see DeviceFlameBuilder); `rng` is this GPU thread's persistent RNG
// stream (one per thread, alive for the whole render - see chaos_kernel.cu).
struct DeviceVarContext {
    double tx, ty, tz;   // mutable: pre_ variations perturb these in place
    double px, py, pz;   // mutable: every variation accumulates into these
    double vvar;
    double a, b, c, d, e, f; // owning xform's affine coefs (c00,c01,c10,c11,c20,c21)
    double p00, p01, p10, p11, p20, p21; // owning xform's post-transform coefs
    // Precalc scratch (populated by kOpPrecalcAngle/SinCos/All before any
    // op that reads them runs - mirrors XForm::precalcAngle/SinCos/All).
    double angle, length, sinA, cosA;
    // Mirrors Variation::color (CPU side: bound to XForm's vc_) - starts
    // equal to the post-color-speed colorCoord and is read/written only by
    // the handful of variations that touch it (falloff2/post_falloff2/
    // pre_falloff2 - see VariationKinds.cpp); every other device variation
    // function ignores this field entirely, in which case it just passes
    // through unchanged, matching the CPU's own vc_-stays-equal-to-
    // colorCoord behavior for every non-DC variation.
    double color;
    const double* params;
    DeviceRng* rng;
};

// ---- Local variations (0..28), ported from XForm.cpp -----------------

__device__ inline void devLinear(DeviceVarContext& c) {
    c.px += c.vvar * c.tx;
    c.py += c.vvar * c.ty;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devFlatten(DeviceVarContext& c) { c.pz = 0; }
__device__ inline void devSinusoidal(DeviceVarContext& c) {
    c.px += c.vvar * sin(c.tx);
    c.py += c.vvar * sin(c.ty);
    c.pz += c.tz * c.vvar;
}
__device__ inline void devSpherical(DeviceVarContext& c) {
    const double r = c.vvar / (c.tx * c.tx + c.ty * c.ty + kDeviceEps);
    c.px += c.tx * r;
    c.py += c.ty * r;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devSwirl(DeviceVarContext& c) {
    const double t = c.tx * c.tx + c.ty * c.ty;
    const double sinr = sin(t), cosr = cos(t);
    c.px += c.vvar * (sinr * c.tx - cosr * c.ty);
    c.py += c.vvar * (cosr * c.tx + sinr * c.ty);
    c.pz += c.tz * c.vvar;
}
__device__ inline void devHorseshoe(DeviceVarContext& c) {
    const double r = c.vvar / (sqrt(c.tx * c.tx + c.ty * c.ty) + kDeviceEps);
    c.px += (c.tx - c.ty) * (c.tx + c.ty) * r;
    c.py += (2 * c.tx * c.ty) * r;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devPolar(DeviceVarContext& c) {
    c.px += (c.vvar / kDevicePi) * c.angle;
    c.py += c.vvar * (sqrt(c.tx * c.tx + c.ty * c.ty) - 1.0);
    c.pz += c.tz * c.vvar;
}
__device__ inline void devDisc(DeviceVarContext& c) {
    const double t = kDevicePi * sqrt(c.tx * c.tx + c.ty * c.ty);
    const double sinr = sin(t), cosr = cos(t);
    const double r = (c.vvar / kDevicePi) * c.angle;
    c.px += sinr * r;
    c.py += cosr * r;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devSpiral(DeviceVarContext& c) {
    double r = c.length + 1e-6;
    const double sinr = sin(r), cosr = cos(r);
    r = c.vvar / r;
    c.px += (c.cosA + sinr) * r;
    c.py += (c.sinA - cosr) * r;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devHyperbolic(DeviceVarContext& c) {
    c.px += c.vvar * c.tx / (c.tx * c.tx + c.ty * c.ty + kDeviceEps);
    c.py += c.vvar * c.ty;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devDiamond(DeviceVarContext& c) { // local index 10, "square"/diamond
    const double sinr = sin(c.length), cosr = cos(c.length);
    c.px += c.vvar * c.sinA * cosr;
    c.py += c.vvar * c.cosA * sinr;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devEyefish(DeviceVarContext& c) {
    const double r = 2 * c.vvar / (sqrt(c.tx * c.tx + c.ty * c.ty) + 1);
    c.px += r * c.tx;
    c.py += r * c.ty;
    c.pz += c.tz * c.vvar;
}
__device__ inline void devBubble(DeviceVarContext& c) {
    double r = (c.tx * c.tx + c.ty * c.ty) / 4 + 1;
    c.pz += c.vvar * (2 / r - 1);
    r = c.vvar / r;
    c.px += r * c.tx;
    c.py += r * c.ty;
}
__device__ inline void devCylinder(DeviceVarContext& c) {
    c.px += c.vvar * sin(c.tx);
    c.py += c.vvar * c.ty;
    c.pz += c.vvar * cos(c.tx);
}
__device__ inline void devNoise(DeviceVarContext& c) {
    const double ang = c.rng->uniformAngle();
    const double sinr = sin(ang), cosr = cos(ang);
    const double s = c.vvar;
    const double r = s * c.rng->uniform01();
    c.px += c.tx * r * cosr;
    c.py += c.ty * r * sinr;
    c.pz += c.tz * s;
}
__device__ inline void devBlur(DeviceVarContext& c) {
    const double ang = c.rng->uniformAngle();
    const double sina = sin(ang), cosa = cos(ang);
    const double s = c.vvar;
    const double z = c.tz;
    const double r = s * c.rng->uniform01();
    c.px += r * cosa;
    c.py += r * sina;
    c.pz += s * z;
}
__device__ inline double devGaussSum(DeviceRng& rng) { // see this file's header comment
    return rng.uniform01() + rng.uniform01() + rng.uniform01() + rng.uniform01() - 2.0;
}
__device__ inline void devGaussianBlur(DeviceVarContext& c) {
    const double ang = c.rng->uniformAngle();
    const double sina = sin(ang), cosa = cos(ang);
    const double s = c.vvar;
    const double z = c.tz;
    const double r = s * devGaussSum(*c.rng);
    c.px += r * cosa;
    c.py += r * sina;
    c.pz += s * z;
}
__device__ inline void devZBlur(DeviceVarContext& c) { c.pz += c.vvar * devGaussSum(*c.rng); }
__device__ inline void devBlur3D(DeviceVarContext& c) {
    const double r = c.vvar * devGaussSum(*c.rng);
    const double angB = c.rng->uniform01() * kDevicePi;
    const double ang = c.rng->uniformAngle();
    const double sinb = sin(angB), cosb = cos(angB);
    const double sina = sin(ang), cosa = cos(ang);
    c.px += r * sinb * cosa;
    c.py += r * sinb * sina;
    c.pz += r * cosb;
}
__device__ inline void devPreBlur(DeviceVarContext& c) {
    const double ang = c.rng->uniformAngle();
    const double sina = sin(ang), cosa = cos(ang);
    const double r = c.vvar * devGaussSum(*c.rng);
    c.tx += r * cosa;
    c.ty += r * sina;
}
__device__ inline void devPreZScale(DeviceVarContext& c) { c.tz *= c.vvar; }
__device__ inline void devPreZTranslate(DeviceVarContext& c) { c.tz += c.vvar; }
__device__ inline void devPreRotateX(DeviceVarContext& c) {
    const double ang = c.vvar * kDevicePi / 2;
    const double s = sin(ang), co = cos(ang);
    const double z = co * c.tz - s * c.ty;
    c.ty = s * c.tz + co * c.ty;
    c.tz = z;
}
__device__ inline void devPreRotateY(DeviceVarContext& c) {
    const double ang = c.vvar * kDevicePi / 2;
    const double s = sin(ang), co = cos(ang);
    const double x = co * c.tx - s * c.tz;
    c.tz = s * c.tx + co * c.tz;
    c.tx = x;
}
__device__ inline void devZScale(DeviceVarContext& c) { c.pz += c.vvar * c.tz; }
__device__ inline void devZTranslate(DeviceVarContext& c) { c.pz += c.vvar; }
__device__ inline void devZCone(DeviceVarContext& c) { c.pz += c.vvar * sqrt(c.tx * c.tx + c.ty * c.ty); }
__device__ inline void devPostRotateX(DeviceVarContext& c) {
    const double ang = c.vvar * kDevicePi / 2;
    const double s = sin(ang), co = cos(ang);
    const double z = co * c.pz - s * c.py;
    c.py = s * c.pz + co * c.py;
    c.pz = z;
}
__device__ inline void devPostRotateY(DeviceVarContext& c) {
    const double ang = c.vvar * kDevicePi / 2;
    const double s = sin(ang), co = cos(ang);
    const double x = co * c.px - s * c.pz;
    c.pz = s * c.px + co * c.pz;
    c.px = x;
}

// ---- Registered variations (kFirstRegisteredKind+N), see VariationKinds.cpp

__device__ inline void devAuger(DeviceVarContext& c) {
    const double freq = c.params[0], weight = c.params[1], scale = c.params[2], sym = c.params[3];
    const double x = c.tx, y = c.ty;
    const double s = sin(freq * x), t = sin(freq * y);
    const double dx = x + weight * (0.5 * scale * t + fabs(x) * t);
    const double dy = y + weight * (0.5 * scale * s + fabs(y) * s);
    c.px += c.vvar * (x + sym * (dx - x));
    c.py += c.vvar * dy;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devBent(DeviceVarContext& c) {
    double nx = c.tx, ny = c.ty;
    if (nx < 0.0) nx *= 2.0;
    if (ny < 0.0) ny /= 2.0;
    c.px += c.vvar * nx;
    c.py += c.vvar * ny;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devBlob(DeviceVarContext& c) {
    const double low = c.params[0], high = c.params[1], waves = c.params[2];
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double angle = atan2(c.tx, c.ty);
    const double bdiff = high - low;
    const double r = rad * (low + bdiff * (0.5 + 0.5 * sin(waves * angle)));
    const double sina = c.tx / rad, cosa = c.ty / rad;
    c.px += c.vvar * sina * r;
    c.py += c.vvar * cosa * r;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devCosine(DeviceVarContext& c) {
    const double ang = c.tx * kDevicePi;
    const double sa = sin(ang), ca = cos(ang);
    c.px += c.vvar * (ca * cosh(c.ty));
    c.py += c.vvar * (-sa * sinh(c.ty));
    c.pz += c.vvar * c.tz;
}
__device__ inline void devCross(DeviceVarContext& c) {
    double r = fabs((c.tx - c.ty) * (c.tx + c.ty) + 1e-6);
    r = c.vvar / r;
    c.px += c.tx * r;
    c.py += c.ty * r;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devExponential(DeviceVarContext& c) {
    const double dx = c.vvar * exp(c.tx - 1.0);
    const double dy = kDevicePi * c.ty;
    c.px += dx * cos(dy);
    c.py += dx * sin(dy);
    c.pz += c.vvar * c.tz;
}
__device__ inline void devFoci(DeviceVarContext& c) {
    const double expx = exp(c.tx) * 0.5;
    const double expnx = 0.25 / expx;
    const double siny = sin(c.ty), cosy = cos(c.ty);
    double tmp = expx + expnx - cosy;
    if (tmp == 0) tmp = 1e-6;
    tmp = c.vvar / tmp;
    c.px += (expx - expnx) * tmp;
    c.py += siny * tmp;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devHeart(DeviceVarContext& c) {
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double angle = atan2(c.tx, c.ty);
    const double heartAngle = rad * angle;
    const double r = c.vvar * rad;
    c.px += r * sin(heartAngle);
    c.py += -r * cos(heartAngle);
    c.pz += c.vvar * c.tz;
}
__device__ inline void devHemisphere(DeviceVarContext& c) {
    const double t = c.vvar / sqrt(c.tx * c.tx + c.ty * c.ty + 1);
    c.px += c.tx * t;
    c.py += c.ty * t;
    c.pz += t;
}
__device__ inline void devLazysusan(DeviceVarContext& c) {
    const double spin = c.params[0], space = c.params[1], twist = c.params[2];
    const double lx = c.params[3], ly = c.params[4];
    const double x = c.tx - lx, y = c.ty + ly;
    double r = sqrt(x * x + y * y);
    if (r < c.vvar) {
        const double angle = atan2(y, x) + spin + twist * (c.vvar - r);
        c.px += c.vvar * (r * cos(angle) + lx);
        c.py += c.vvar * (r * sin(angle) - ly);
    } else {
        r = 1.0 + space / (r + 1e-6);
        c.px += c.vvar * (r * x + lx);
        c.py += c.vvar * (r * y - ly);
    }
    c.pz += c.vvar * c.tz;
}
__device__ inline void devMobius(DeviceVarContext& c) {
    const double reA = c.params[0], imA = c.params[1], reB = c.params[2], imB = c.params[3];
    const double reC = c.params[4], imC = c.params[5], reD = c.params[6], imD = c.params[7];
    const double uRe = reA * c.tx - imA * c.ty + reB;
    const double uIm = reA * c.ty + imA * c.tx + imB;
    const double vRe = reC * c.tx - imC * c.ty + reD;
    const double vIm = reC * c.ty + imC * c.tx + imD;
    const double vDenom = vRe * vRe + vIm * vIm;
    c.px += c.vvar * (uRe * vRe + uIm * vIm) / vDenom;
    c.py += c.vvar * (uIm * vRe - uRe * vIm) / vDenom;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devPdj(DeviceVarContext& c) {
    const double a = c.params[0], b = c.params[1], pc = c.params[2], d = c.params[3];
    c.px += c.vvar * (sin(a * c.ty) - cos(b * c.tx));
    c.py += c.vvar * (sin(pc * c.tx) - cos(d * c.ty));
    c.pz += c.vvar * c.tz;
}
__device__ inline void devPopcorn(DeviceVarContext& c) {
    const double dxv = tan(3.0 * c.ty);
    const double dyv = tan(3.0 * c.tx);
    const double nx = c.tx + c.e * sin(dxv);
    const double ny = c.ty + c.f * sin(dyv);
    c.px += c.vvar * nx;
    c.py += c.vvar * ny;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devPower(DeviceVarContext& c) {
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double sina = c.tx / rad, cosa = c.ty / rad;
    const double r = c.vvar * pow(rad, sina);
    c.px += r * cosa;
    c.py += r * sina;
    c.pz += c.vvar * c.tz;
}
__device__ inline void devPreSinusoidal(DeviceVarContext& c) {
    c.tx = c.vvar * sin(c.tx);
    c.ty = c.vvar * sin(c.ty);
    c.tz = c.vvar * c.tz;
}
__device__ inline void devPreSpherical(DeviceVarContext& c) {
    const double r = c.vvar / (c.tx * c.tx + c.ty * c.ty + 10e-6);
    c.tx = c.tx * r;
    c.ty = c.ty * r;
    c.tz = c.vvar * c.tz;
}
__device__ inline void devSecant2(DeviceVarContext& c) {
    const double rad = c.vvar * sqrt(c.tx * c.tx + c.ty * c.ty);
    const double cr = cos(rad);
    const double icr = 1.0 / cr;
    c.px += c.vvar * c.tx;
    c.py += (cr < 0.0) ? c.vvar * (icr + 1.0) : c.vvar * (icr - 1.0);
    c.pz += c.vvar * c.tz;
}
__device__ inline void devSeparation(DeviceVarContext& c) {
    const double sx = c.params[0], sy = c.params[1], sxi = c.params[2], syi = c.params[3];
    if (c.tx > 0.0) {
        c.px += c.vvar * (sqrt(c.tx * c.tx + sx * sx) - c.tx * sxi);
    } else {
        c.px -= c.vvar * (sqrt(c.tx * c.tx + sx * sx) + c.tx * sxi);
    }
    if (c.ty > 0.0) {
        c.py += c.vvar * (sqrt(c.ty * c.ty + sy * sy) - c.ty * syi);
    } else {
        c.py -= c.vvar * (sqrt(c.ty * c.ty + sy * sy) + c.ty * syi);
    }
    c.pz += c.vvar * c.tz;
}
__device__ inline void devSplits(DeviceVarContext& c) {
    const double sx = c.params[0], sy = c.params[1];
    c.px += (c.tx >= 0.0) ? c.vvar * (c.tx + sx) : c.vvar * (c.tx - sx);
    c.py += (c.ty >= 0.0) ? c.vvar * (c.ty + sy) : c.vvar * (c.ty - sy);
    c.pz += c.vvar * c.tz;
}
__device__ inline void devTangent(DeviceVarContext& c) {
    c.px += c.vvar * sin(c.tx) / cos(c.ty);
    c.py += c.vvar * tan(c.ty);
    c.pz += c.vvar * c.tz;
}
__device__ inline void devWaves2(DeviceVarContext& c) {
    const double fx = c.params[0], fy = c.params[1], fz = c.params[2];
    const double sx = c.params[3], sy = c.params[4], sz = c.params[5];
    c.px += c.vvar * (c.tx + sx * sin(c.ty * fx));
    c.py += c.vvar * (c.ty + sy * sin(c.tx * fy));
    c.pz += c.vvar * (c.tz + sz * sin(sqrt(c.tx * c.tx + c.ty * c.ty) * fz));
}

// ---- Batch 2: "prepare()-only" registered variations ------------------
//
// Each of these caches a few derived constants in its CPU-side prepare()
// (see src/core/variations/Var*.cpp), computed once per render from the
// variation's own raw parameters (never touched per-point). Rather than
// threading a second "derived params" upload path through
// DeviceFlameBuilder just for this batch, every device function below
// simply recomputes that handful of constants inline, every call, straight
// from the same raw parameters uploaded for every other variation - a few
// extra flops per point, invisible next to the trig/sqrt/log this math
// already does, in exchange for zero additional host-side plumbing. See
// VariationKinds.cpp for the name/param-list table these read from.

__device__ inline double devSqrtSafe(double x) { return x < 0.0 ? 0.0 : sqrt(x); }
__device__ inline double devFrac(double x) { return x - trunc(x); } // Delphi's Frac(): truncates toward zero

__device__ inline void devBipolar(DeviceVarContext& c) {
    const double shift = c.params[0];
    const double halfPi = kDevicePi / 2.0;
    const double v4 = c.vvar * 0.15915494309189533576888376337251;
    const double v = c.vvar * 0.636619772367581343075535053490061;
    const double s = -halfPi * shift;
    const double x2y2 = c.tx * c.tx + c.ty * c.ty;
    double y = 0.5 * atan2(2.0 * c.ty, x2y2 - 1.0) + s;
    if (y > halfPi) {
        y = -halfPi + fmod(y + halfPi, kDevicePi);
    } else if (y < -halfPi) {
        y = halfPi - fmod(halfPi - y, kDevicePi);
    }
    const double t = x2y2 + 1.0;
    const double x2 = 2.0 * c.tx;
    const double numer = t + x2, denom = t - x2;
    if (denom == 0 || numer / denom <= 0) return;
    c.px += v4 * log(numer / denom);
    c.py += v * y;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devBwraps(DeviceVarContext& c) {
    const double cellsize = c.params[0], space = c.params[1], gain = c.params[2];
    const double innerTwist = c.params[3], outerTwist = c.params[4];
    const double vx = c.tx, vy = c.ty;
    if (cellsize == 0.0) {
        c.px += c.vvar * c.tx;
        c.py += c.vvar * c.ty;
        c.pz += c.vvar * c.tz;
        return;
    }
    const double radius = 0.5 * (cellsize / (1.0 + space * space));
    const double g2 = gain * gain / (radius + 1e-6) + 1e-6;
    double maxBubble = g2 * radius;
    if (maxBubble > 2.0) {
        maxBubble = 1.0;
    } else {
        maxBubble = maxBubble * (1.0 / (maxBubble * maxBubble / 4.0 + 1.0));
    }
    const double r2 = radius * radius;
    const double rfactor = radius / maxBubble;
    const double cx = (floor(vx / cellsize) + 0.5) * cellsize;
    const double cy = (floor(vy / cellsize) + 0.5) * cellsize;
    double lx = vx - cx, ly = vy - cy;
    if (lx * lx + ly * ly > r2) {
        c.px += c.vvar * c.tx;
        c.py += c.vvar * c.ty;
        c.pz += c.vvar * c.tz;
        return;
    }
    lx *= g2;
    ly *= g2;
    double r = rfactor / ((lx * lx + ly * ly) / 4.0 + 1);
    lx *= r;
    ly *= r;
    r = (lx * lx + ly * ly) / r2;
    const double theta = innerTwist * (1.0 - r) + outerTwist * r;
    const double sinT = sin(theta), cosT = cos(theta);
    c.px += c.vvar * (cx + cosT * lx + sinT * ly);
    c.py += c.vvar * (cy - sinT * lx + cosT * ly);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devPreBwraps(DeviceVarContext& c) {
    const double cellsize = c.params[0], space = c.params[1], gain = c.params[2];
    const double innerTwist = c.params[3], outerTwist = c.params[4];
    const double vx = c.tx, vy = c.ty;
    if (cellsize == 0.0) return; // leaves tx/ty/tz untouched - matches VarPreBwraps::calc()
    const double radius = 0.5 * (cellsize / (1.0 + space * space));
    const double g2 = gain * gain / (radius + 1e-6) + 1e-6;
    double maxBubble = g2 * radius;
    if (maxBubble > 2.0) {
        maxBubble = 1.0;
    } else {
        maxBubble = maxBubble * (1.0 / (maxBubble * maxBubble / 4.0 + 1.0));
    }
    const double r2 = radius * radius;
    const double rfactor = radius / maxBubble;
    const double cx = (floor(vx / cellsize) + 0.5) * cellsize;
    const double cy = (floor(vy / cellsize) + 0.5) * cellsize;
    double lx = vx - cx, ly = vy - cy;
    if (lx * lx + ly * ly > r2) return;
    lx *= g2;
    ly *= g2;
    double r = rfactor / ((lx * lx + ly * ly) / 4.0 + 1);
    lx *= r;
    ly *= r;
    r = (lx * lx + ly * ly) / r2;
    const double theta = innerTwist * (1.0 - r) + outerTwist * r;
    const double sinT = sin(theta), cosT = cos(theta);
    c.tx = c.vvar * (cx + cosT * lx + sinT * ly);
    c.ty = c.vvar * (cy - sinT * lx + cosT * ly);
    c.tz = c.vvar * c.tz;
}

__device__ inline void devPostBwraps(DeviceVarContext& c) {
    const double cellsize = c.params[0], space = c.params[1], gain = c.params[2];
    const double innerTwist = c.params[3], outerTwist = c.params[4];
    const double vx = c.px, vy = c.py;
    if (cellsize == 0.0) return; // leaves px/py/pz untouched - matches VarPostBwraps::calc()
    const double radius = 0.5 * (cellsize / (1.0 + space * space));
    const double g2 = gain * gain / (radius + 1e-6) + 1e-6;
    double maxBubble = g2 * radius;
    if (maxBubble > 2.0) {
        maxBubble = 1.0;
    } else {
        maxBubble = maxBubble * (1.0 / (maxBubble * maxBubble / 4.0 + 1.0));
    }
    const double r2 = radius * radius;
    const double rfactor = radius / maxBubble;
    const double cx = (floor(vx / cellsize) + 0.5) * cellsize;
    const double cy = (floor(vy / cellsize) + 0.5) * cellsize;
    double lx = vx - cx, ly = vy - cy;
    if (lx * lx + ly * ly > r2) return;
    lx *= g2;
    ly *= g2;
    double r = rfactor / ((lx * lx + ly * ly) / 4.0 + 1);
    lx *= r;
    ly *= r;
    r = (lx * lx + ly * ly) / r2;
    const double theta = innerTwist * (1.0 - r) + outerTwist * r;
    const double sinT = sin(theta), cosT = cos(theta);
    c.px = c.vvar * (cx + cosT * lx + sinT * ly);
    c.py = c.vvar * (cy - sinT * lx + cosT * ly);
    c.pz = c.vvar * c.pz;
}

__device__ inline void devCurl3D(DeviceVarContext& c) {
    const double cx = c.params[0], cy = c.params[1], cz = c.params[2];
    const double c2x = 2 * cx, c2y = 2 * cy, c2z = 2 * cz;
    const double c2 = cx * cx + cy * cy + cz * cz;
    const double r2 = c.tx * c.tx + c.ty * c.ty + c.tz * c.tz;
    const double r = c.vvar / (r2 * c2 + c2x * c.tx - c2y * c.ty + c2z * c.tz + 1);
    c.px += r * (c.tx + cx * r2);
    c.py += r * (c.ty - cy * r2);
    c.pz += r * (c.tz + cz * r2);
}

__device__ inline void devPostCurl3D(DeviceVarContext& c) {
    const double cx = c.params[0], cy = c.params[1], cz = c.params[2];
    const double cxs = c.vvar * cx, cys = c.vvar * cy, czs = c.vvar * cz;
    const double c2x = 2 * cxs, c2y = 2 * cys, c2z = 2 * czs;
    const double cSum2 = cxs * cxs + cys * cys + czs * czs;
    const double x = fmax(-1e100, fmin(c.px, 1e100));
    const double y = fmax(-1e100, fmin(c.py, 1e100));
    const double z = fmax(-1e100, fmin(c.pz, 1e100));
    const double r2 = x * x + y * y + z * z;
    const double r = 1.0 / (r2 * cSum2 + c2x * x - c2y * y + c2z * z + 1);
    c.px = r * (x + cxs * r2);
    c.py = r * (y + cys * r2);
    c.pz = r * (z + czs * r2);
}

__device__ inline void devElliptic(DeviceVarContext& c) {
    const double v = c.vvar / (kDevicePi / 2.0);
    const double tmp = c.ty * c.ty + c.tx * c.tx + 1.0;
    const double x2 = 2.0 * c.tx;
    const double xmax = 0.5 * (sqrt(tmp + x2) + sqrt(tmp - x2));
    const double ea = c.tx / xmax;
    const double eb = devSqrtSafe(1.0 - ea * ea);
    c.pz += c.vvar * c.tz;
    c.px += v * atan2(ea, eb);
    if (c.ty > 0) {
        c.py += v * log(xmax + devSqrtSafe(xmax - 1.0));
    } else {
        c.py -= v * log(xmax + devSqrtSafe(xmax - 1.0));
    }
}

__device__ inline void devEscher(DeviceVarContext& c) {
    const double beta = c.params[0];
    double escD = sin(beta);
    double escC = cos(beta);
    escC = 0.5 * (1.0 + escC);
    escD = 0.5 * escD;
    const double ang = atan2(c.ty, c.tx);
    const double lnr = 0.5 * log(c.tx * c.tx + c.ty * c.ty);
    const double m = c.vvar * exp(escC * lnr - escD * ang);
    const double angle = escC * ang + escD * lnr;
    c.px += m * cos(angle);
    c.py += m * sin(angle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devFan(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double dxv = kDevicePi * (c.e * c.e + kEps10);
    const double dyv = c.f;
    const double dx2 = dxv / 2.0;
    double ang = atan2(c.tx, c.ty);
    const double rad = c.vvar * sqrt(c.tx * c.tx + c.ty * c.ty);
    ang += (fmod(ang + dyv, dxv) > dx2) ? -dx2 : dx2;
    c.px += rad * cos(ang);
    c.py += rad * sin(ang);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devFan2(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double fanX = c.params[0], fanY = c.params[1];
    const double dyv = fanY;
    const double dxv = kDevicePi * (fanX * fanX + kEps10);
    const double dx2 = dxv / 2;
    const double angle = atan2(c.tx, c.ty);
    const double fanAngle = (devFrac((angle + dyv) / dxv) > 0.5) ? angle - dx2 : angle + dx2;
    const double r = c.vvar * sqrt(c.tx * c.tx + c.ty * c.ty);
    c.px += r * cos(fanAngle);
    c.py += r * sin(fanAngle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devLog(DeviceVarContext& c) {
    const double base = c.params[0];
    const double denom = 0.5 / log(base);
    c.px += c.vvar * log(c.tx * c.tx + c.ty * c.ty) * denom;
    c.py += c.vvar * atan2(c.ty, c.tx);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devLoonie(DeviceVarContext& c) {
    const double sqrVar = c.vvar * c.vvar;
    const double r2 = c.tx * c.tx + c.ty * c.ty;
    if (r2 < sqrVar && r2 != 0) {
        const double r = c.vvar * sqrt(sqrVar / r2 - 1.0);
        c.px += r * c.tx;
        c.py += r * c.ty;
    } else {
        c.px += c.vvar * c.tx;
        c.py += c.vvar * c.ty;
    }
    c.pz += c.vvar * c.tz;
}

__device__ inline void devNGon(DeviceVarContext& c) {
    const double sides = c.params[0], power = c.params[1], circle = c.params[2], corners = c.params[3];
    const double cpower = -0.5 * power;
    const double csides = 2.0 * kDevicePi / sides;
    const double csidesinv = 1.0 / csides;
    double rFactor;
    if (c.tx == 0 && c.ty == 0) {
        rFactor = 0;
    } else {
        rFactor = pow(c.tx * c.tx + c.ty * c.ty, cpower);
    }
    const double theta = atan2(c.ty, c.tx);
    double phi = theta - csides * floor(theta * csidesinv);
    if (phi > 0.5 * csides) phi -= csides;
    const double amp = (corners * (1.0 / cos(phi) - 1.0) + circle) * c.vvar * rFactor;
    c.px += amp * c.tx;
    c.py += amp * c.ty;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devPerspective(DeviceVarContext& c) {
    const double angle = c.params[0], dist = c.params[1];
    const double ang = angle * kDevicePi / 2.0;
    const double vsin = sin(ang);
    const double vfcos = dist * cos(ang);
    const double t = 1.0 / (dist - c.ty * vsin);
    c.px += c.vvar * dist * c.tx * t;
    c.py += c.vvar * vfcos * c.ty * t;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devPolar2(DeviceVarContext& c) {
    const double p2vv = c.vvar / kDevicePi;
    const double p2vv2 = p2vv * 0.5;
    c.py += p2vv2 * log(c.tx * c.tx + c.ty * c.ty);
    c.px += p2vv * atan2(c.tx, c.ty);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devPostCurl(DeviceVarContext& c) {
    const double c1 = c.params[0], c2p = c.params[1];
    const double c1s = c1 * c.vvar;
    const double c2s = c2p * c.vvar;
    const double c22 = 2 * c2s;
    const double x = c.px, y = c.py;
    const double re = 1 + c1s * x + c2s * (x * x - y * y);
    const double im = c1s * y + c22 * x * y;
    const double r = re * re + im * im;
    c.px = (x * re + y * im) / r;
    c.py = (y * re - x * im) / r;
    // pz deliberately untouched - matches VarPostCurl::calc() (no FPz write)
}

__device__ inline void devPreDisc(DeviceVarContext& c) {
    const double vvarByPi = c.vvar / kDevicePi;
    const double t = kDevicePi * sqrt(c.tx * c.tx + c.ty * c.ty);
    const double sinr = sin(t), cosr = cos(t);
    const double r = vvarByPi * atan2(c.tx, c.ty);
    c.tx = sinr * r;
    c.ty = cosr * r;
    c.tz = c.vvar * c.tz;
}

__device__ inline void devRings(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double dxr = c.e * c.e + kEps10;
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double sina = c.tx / rad, cosa = c.ty / rad;
    const double r = c.vvar * (fmod(rad + dxr, 2.0 * dxr) - dxr + rad * (1.0 - dxr));
    c.px += r * cosa;
    c.py += r * sina;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devRings2(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double val = c.params[0];
    const double dxr = val * val + kEps10;
    const double length = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double r = c.vvar * (2 - dxr * (trunc((length / dxr + 1) / 2) * 2 / length + 1));
    c.px += r * c.tx;
    c.py += r * c.ty;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devScry(DeviceVarContext& c) {
    const double v = (c.vvar == 0) ? 1.0 / 1e-6 : 1.0 / c.vvar;
    const double t = c.tx * c.tx + c.ty * c.ty;
    const double r = 1.0 / (sqrt(t) * (t + v));
    c.px += c.tx * r;
    c.py += c.ty * r;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devWaves(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double dx2r = 1.0 / (c.e * c.e + kEps10);
    const double dy2r = 1.0 / (c.f * c.f + kEps10);
    const double nx = c.tx + c.c * sin(c.ty * dx2r);
    const double ny = c.ty + c.d * sin(c.tx * dy2r);
    c.px += c.vvar * nx;
    c.py += c.vvar * ny;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devWedge(DeviceVarContext& c) {
    constexpr double kOneOverTwoPi = 0.15915494309189533576888376337251;
    const double angle = c.params[0], hole = c.params[1], count = c.params[2], swirl = c.params[3];
    const double compFac = 1.0 - angle * count * kOneOverTwoPi;
    double r = sqrt(c.tx * c.tx + c.ty * c.ty);
    double ang = atan2(c.ty, c.tx) + swirl * r;
    const int cell = static_cast<int>(floor((count * ang + kDevicePi) * kOneOverTwoPi));
    ang = ang * compFac + cell * angle;
    r = c.vvar * (r + hole);
    c.px += r * cos(ang);
    c.py += r * sin(ang);
    c.pz += c.vvar * c.tz;
}

// ---- Batch 3: RNG-using / selectCalcFunction()-specialized variations -

__device__ inline void devArch(DeviceVarContext& c) {
    const double ang = c.rng->uniform01() * c.vvar * kDevicePi;
    const double sinr = sin(ang), cosr = cos(ang);
    c.px += c.vvar * sinr;
    c.py += c.vvar * (sinr * sinr) / cosr;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devBlade(DeviceVarContext& c) {
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double r = c.rng->uniform01() * c.vvar * rad;
    const double sinr = sin(r), cosr = cos(r);
    c.px += c.vvar * c.tx * (cosr + sinr);
    c.py += c.vvar * c.tx * (cosr - sinr);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devBlurCircle(DeviceVarContext& c) {
    const double pi4 = kDevicePi / 4.0;
    const double x = 2.0 * c.rng->uniform01() - 1.0;
    const double y = 2.0 * c.rng->uniform01() - 1.0;
    const double absx = fabs(x), absy = fabs(y);
    double side, perimeter;
    if (absx >= absy) {
        perimeter = (x >= absy) ? (absx + y) : (5.0 * absx - y);
        side = absx;
    } else {
        perimeter = (y >= absx) ? (3.0 * absy - x) : (7.0 * absy + x);
        side = absy;
    }
    const double r = c.vvar * side;
    const double angle = pi4 * perimeter / side - pi4;
    c.px += r * cos(angle);
    c.py += r * sin(angle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devBlurPixelize(DeviceVarContext& c) {
    const double size = c.params[0], scale = c.params[1];
    const double invSize = 1.0 / size;
    const double v = c.vvar * size;
    const double x = floor(c.tx * invSize);
    const double y = floor(c.ty * invSize);
    c.px += v * (x + scale * (c.rng->uniform01() - 0.5) + 0.5);
    c.py += v * (y + scale * (c.rng->uniform01() - 0.5) + 0.5);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devBlurZoom(DeviceVarContext& c) {
    const double length = c.params[0], zx = c.params[1], zy = c.params[2];
    const double z = 1.0 + length * c.rng->uniform01();
    c.px += c.vvar * ((c.tx - zx) * z + zx);
    c.py += c.vvar * ((c.ty - zy) * z - zy);
    c.pz += c.vvar * c.tz;
}

// Shared by crop/post_crop/pre_crop - normalizes x0/x1, y0/y1 and computes
// the scatter half-width/height inline (see this file's header comment on
// why "prepare()-only" derived constants are recomputed per-call rather
// than uploaded separately - the same reasoning applies here even though
// this batch also uses the RNG).
__device__ inline void devCropCommon(double x0, double y0, double x1, double y1, double s, DeviceRng& rng,
                                      double& x, double& y, double zero) {
    double normX0 = (x0 < x1) ? x0 : x1, normX1 = (x0 < x1) ? x1 : x0;
    double normY0 = (y0 < y1) ? y0 : y1, normY1 = (y0 < y1) ? y1 : y0;
    const double w = (normX1 - normX0) * 0.5 * s;
    const double h = (normY1 - normY0) * 0.5 * s;
    if ((x < normX0 || x > normX1 || y < normY0 || y > normY1) && zero != 0) {
        x = 0;
        y = 0;
    } else {
        if (x < normX0) x = normX0 + rng.uniform01() * w;
        else if (x > normX1) x = normX1 - rng.uniform01() * w;
        if (y < normY0) y = normY0 + rng.uniform01() * h;
        else if (y > normY1) y = normY1 - rng.uniform01() * h;
    }
}

__device__ inline void devCrop(DeviceVarContext& c) {
    const double x0 = c.params[0], y0 = c.params[1], x1 = c.params[2], y1 = c.params[3];
    const double s = c.params[4], zero = c.params[5];
    double x = c.tx, y = c.ty;
    devCropCommon(x0, y0, x1, y1, s, *c.rng, x, y, zero);
    c.px += c.vvar * x;
    c.py += c.vvar * y;
    c.pz += c.vvar * c.tz;
}

__device__ inline void devPostCrop(DeviceVarContext& c) {
    const double x0 = c.params[0], y0 = c.params[1], x1 = c.params[2], y1 = c.params[3];
    const double s = c.params[4], zero = c.params[5];
    double x = c.px, y = c.py;
    devCropCommon(x0, y0, x1, y1, s, *c.rng, x, y, zero);
    c.px = c.vvar * x;
    c.py = c.vvar * y;
    c.pz = c.vvar * c.pz;
}

__device__ inline void devPreCrop(DeviceVarContext& c) {
    const double x0 = c.params[0], y0 = c.params[1], x1 = c.params[2], y1 = c.params[3];
    const double s = c.params[4], zero = c.params[5];
    double x = c.tx, y = c.ty;
    devCropCommon(x0, y0, x1, y1, s, *c.rng, x, y, zero);
    c.tx = c.vvar * x;
    c.ty = c.vvar * y;
    c.tz = c.vvar * c.tz;
}

// Curl's selectCalcFunction (CPU side) picks calcZeroC1/calcZeroC2/
// calcZeroC2C1 as verified-algebraically-equivalent fast paths when c1
// and/or c2 are ~zero (see VarCurl.cpp's own class comment) - multiplying
// through by a zero coefficient in the general formula below already
// produces the same result, so the device side always uses the general
// formula and never needs the specialization.
__device__ inline void devCurl(DeviceVarContext& c) {
    const double c1 = c.params[0], c2 = c.params[1];
    const double c2x2 = 2 * c2;
    const double re = 1 + c1 * c.tx + c2 * (c.tx * c.tx - c.ty * c.ty);
    const double im = c1 * c.ty + c2x2 * c.tx * c.ty;
    const double r = c.vvar / (re * re + im * im);
    c.px += (c.tx * re + c.ty * im) * r;
    c.py += (c.ty * re - c.tx * im) * r;
    c.pz += c.vvar * c.tz;
}

// Rectangles' specialization IS functionally required (avoids dividing by
// a zero rectangles_x/rectangles_y - see VarRectangles.h's own comment),
// so unlike Curl this guard is replicated here, not skipped.
__device__ inline void devRectangles(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double rx = c.params[0], ry = c.params[1];
    const bool zx = fabs(rx) < kEps10, zy = fabs(ry) < kEps10;
    c.px += zx ? (c.vvar * c.tx) : (c.vvar * ((2 * floor(c.tx / rx) + 1) * rx - c.tx));
    c.py += zy ? (c.vvar * c.ty) : (c.vvar * ((2 * floor(c.ty / ry) + 1) * ry - c.ty));
    c.pz += c.vvar * c.tz;
}

__device__ inline void devEpispiral(DeviceVarContext& c) {
    const double n = c.params[0], thickness = c.params[1], holes = c.params[2];
    const double theta = atan2(c.ty, c.tx);
    const double t = (c.rng->uniform01() * thickness) * (1.0 / cos(n * theta)) - holes;
    if (fabs(t) == 0.0) return;
    c.px += c.vvar * t * cos(theta);
    c.py += c.vvar * t * sin(theta);
}

__device__ inline void devPie(DeviceVarContext& c) {
    const double slices = c.params[0], rotation = c.params[1], thickness = c.params[2];
    const int slice = static_cast<int>(c.rng->uniform01() * slices + 0.5);
    const double angle = rotation + kDeviceTwoPi * (slice + c.rng->uniform01() * thickness) / slices;
    const double r = c.vvar * c.rng->uniform01();
    c.px += r * cos(angle);
    c.py += r * sin(angle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devRays(DeviceVarContext& c) {
    constexpr double kEps10 = 1e-10;
    const double ang = c.vvar * c.rng->uniform01() * kDevicePi;
    const double r = c.vvar / (c.tx * c.tx + c.ty * c.ty + kEps10);
    const double tanr = c.vvar * tan(ang) * r;
    c.px += tanr * cos(c.tx);
    c.py += tanr * sin(c.ty);
    c.pz += c.vvar * c.tz;
}

__device__ inline bool devIsBadValue(double x) { return isnan(x) || x > 1e10 || x < -1e10; }

__device__ inline void devTwintrian(DeviceVarContext& c) {
    const double rad = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double r = c.rng->uniform01() * c.vvar * rad;
    const double sinr = sin(r), cosr = cos(r);
    double diff = log10(sinr * sinr) + cosr;
    if (devIsBadValue(diff)) diff = -30.0;
    c.px += c.vvar * c.tx * diff;
    c.py += c.vvar * c.tx * (diff - sinr * kDevicePi);
    c.pz += c.vvar * c.tz;
}

// Julian/JuliaScope/Julia3Dz/Julia3Djf's selectCalcFunction specializations
// (CPU side) are all verified-algebraically-equivalent closed forms for
// specific integer power values (+-1, +-2) - each doc-commented as such in
// its own Var*.cpp (e.g. VarJulian.h: "pick a cheaper specialized calc
// path... " - cheaper, not different). The device side always uses each
// one's general formula below and never needs the specialization, the same
// way it's already safe to do for Curl/RadialBlur above/below.
__device__ inline void devJulian(DeviceVarContext& c) {
    const double power = c.params[0], dist = c.params[1];
    const int n = static_cast<int>(power);
    const int absN = abs(n);
    const double cN = dist / n / 2.0;
    const double angle = (atan2(c.ty, c.tx) + kDeviceTwoPi * c.rng->uniformInt(absN)) / n;
    const double r = c.vvar * pow(c.tx * c.tx + c.ty * c.ty, cN);
    c.px += r * cos(angle);
    c.py += r * sin(angle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devJuliaScope(DeviceVarContext& c) {
    const double power = c.params[0], dist = c.params[1];
    const int n = static_cast<int>(power);
    const int rN = abs(n);
    const double cn = dist / n / 2.0;
    const int rnd = c.rng->uniformInt(rN);
    const double baseAngle = atan2(c.ty, c.tx);
    const double angle = ((rnd & 1) == 0) ? (kDeviceTwoPi * rnd + baseAngle) / n : (kDeviceTwoPi * rnd - baseAngle) / n;
    const double r = c.vvar * pow(c.tx * c.tx + c.ty * c.ty, cn);
    c.px += r * cos(angle);
    c.py += r * sin(angle);
    c.pz += c.vvar * c.tz;
}

__device__ inline void devJulia3Dz(DeviceVarContext& c) {
    const double power = c.params[0];
    const int n = static_cast<int>(power);
    const int absN = abs(n);
    const double cN = 1.0 / n / 2.0;
    const double r2d = c.tx * c.tx + c.ty * c.ty;
    const double r = c.vvar * pow(r2d, cN);
    c.pz += r * c.tz / (sqrt(r2d) * absN);
    const double angle = (atan2(c.ty, c.tx) + kDeviceTwoPi * c.rng->uniformInt(absN)) / n;
    c.px += r * cos(angle);
    c.py += r * sin(angle);
}

__device__ inline void devJulia3Djf(DeviceVarContext& c) {
    const double power = c.params[0];
    const int n = static_cast<int>(power);
    const int absN = abs(n);
    const double cN = (1.0 / n - 1.0) / 2.0;
    const double z = c.tz / absN;
    const double r2d = c.tx * c.tx + c.ty * c.ty;
    const double r = c.vvar * pow(r2d + z * z, cN);
    c.pz += r * z;
    const double tmp = r * sqrt(r2d);
    const double angle = (atan2(c.ty, c.tx) + kDeviceTwoPi * c.rng->uniformInt(absN)) / n;
    c.px += tmp * cos(angle);
    c.py += tmp * sin(angle);
}

// Ported from VarSinhPow.cpp - see that file's doc comment for the map
// (T_c(z) = z^p + sinh(c^q)) and its provenance. No RNG, no cached
// prepare()-derived constant: sinh(c^q) is recomputed inline from the raw
// params every call, same convention as the Batch 2 "prepare()-only"
// variations above (see VariationKinds.h's kSinhPow comment).
__device__ inline void devSinhPow(DeviceVarContext& c) {
    const double power = c.params[0], q = c.params[1], cx = c.params[2], cy = c.params[3];

    const double cr2 = cx * cx + cy * cy;
    const double cMag = pow(cr2, q / 2.0);
    const double cAngle = q * atan2(cy, cx);
    const double wRe = cMag * cos(cAngle);
    const double wIm = cMag * sin(cAngle);
    const double kRe = sinh(wRe) * cos(wIm);
    const double kIm = cosh(wRe) * sin(wIm);

    const double zr2 = c.tx * c.tx + c.ty * c.ty;
    const double zMag = pow(zr2, power / 2.0);
    const double zAngle = power * atan2(c.ty, c.tx);

    c.px += c.vvar * (zMag * cos(zAngle) + kRe);
    c.py += c.vvar * (zMag * sin(zAngle) + kIm);
    c.pz += c.vvar * c.tz;
}

// z^p via the principal branch - shared helper for devSinhPowIA below (see
// VarSinhPowIA.cpp's identical complexPow()).
__device__ inline void devComplexPow(double re, double im, double p, double& outRe, double& outIm) {
    const double r2 = re * re + im * im;
    const double mag = pow(r2, p / 2.0);
    const double ang = p * atan2(im, re);
    outRe = mag * cos(ang);
    outIm = mag * sin(ang);
}

// Ported from VarSinhPowIA.cpp - sinhpow's "(b)" companion: one full step of
// the paper's IA iteration (x/w/y/z' below) per point, four T_c evaluations
// instead of sinhpow's one. No RNG, no cached prepare()-derived constant -
// same convention as devSinhPow above.
__device__ inline void devSinhPowIA(DeviceVarContext& c) {
    const double power = c.params[0], q = c.params[1], cx = c.params[2], cy = c.params[3];
    const double alpha = c.params[4], beta = c.params[5], gamma = c.params[6], lambda = c.params[7];

    double wRe, wIm;
    devComplexPow(cx, cy, q, wRe, wIm);
    const double kRe = sinh(wRe) * cos(wIm);
    const double kIm = cosh(wRe) * sin(wIm);

    const double zRe = c.tx, zIm = c.ty;
    double tcZRe, tcZIm;
    devComplexPow(zRe, zIm, power, tcZRe, tcZIm);
    tcZRe += kRe;
    tcZIm += kIm;

    const double xRe = (alpha + beta) * zRe + gamma * tcZRe;
    const double xIm = (alpha + beta) * zIm + gamma * tcZIm;
    double tcXRe, tcXIm;
    devComplexPow(xRe, xIm, power, tcXRe, tcXIm);
    tcXRe += kRe;
    tcXIm += kIm;

    const double sRe = (1.0 - lambda) * tcXRe + lambda * tcZRe;
    const double sIm = (1.0 - lambda) * tcXIm + lambda * tcZIm;
    double yRe, yIm;
    devComplexPow(sRe, sIm, power, yRe, yIm);
    yRe += kRe;
    yIm += kIm;

    double zNextRe, zNextIm;
    devComplexPow(yRe, yIm, power, zNextRe, zNextIm);
    zNextRe += kRe;
    zNextIm += kIm;

    c.px += c.vvar * zNextRe;
    c.py += c.vvar * zNextIm;
    c.pz += c.vvar * c.tz;
}

// RadialBlur's selectCalcFunction specializations (calcZoom/calcSpin) are
// also verified-algebraically-equivalent limits (spinVar/zoomVar -> 0 - see
// VarRadialBlur.h's own comment), so the general formula alone suffices.
// The CPU side's gaussRnd-style rnd_[4]/n_ ring buffer is replaced with 4
// fresh draws per call, same simplification and same rationale as the
// local gaussian_blur/zblur/blur3D/pre_blur variations (see this file's own
// top-of-file comment).
__device__ inline void devRadialBlur(DeviceVarContext& c) {
    const double angleParam = c.params[0];
    const double spinVar = c.vvar * sin(angleParam * kDevicePi / 2);
    const double zoomVar = c.vvar * cos(angleParam * kDevicePi / 2);
    const double rndG = devGaussSum(*c.rng);
    const double ra = sqrt(c.tx * c.tx + c.ty * c.ty);
    const double angle = atan2(c.ty, c.tx) + spinVar * rndG;
    const double sina = sin(angle), cosa = cos(angle);
    const double rz = zoomVar * rndG - 1;
    c.px += ra * cosa + rz * c.tx;
    c.py += ra * sina + rz * c.ty;
    c.pz += c.vvar * c.tz;
}

// Falloff2/PostFalloff2/PreFalloff2's blurtype dispatch is a genuine
// 3-formula behavioral branch (Uniform/Radial/Gaussian falloff blur), not
// an optimization - replicated here via `mode`, read from params like any
// other named parameter (falloff2_type/post_falloff2_type/
// pre_falloff2_type). All three also read/write ctx.color, the one place
// in this whole batch that does - see DeviceVarContext's own comment.
__device__ inline double devFalloffDist(double dx, double dy, double dz, double invert, double mindist,
                                         double rmax) {
    double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (invert != 0) dist = 1 - dist;
    if (dist < 0) dist = 0;
    dist = (dist - mindist) * rmax;
    if (dist < 0) dist = 0;
    return dist;
}

// `read`/`write` select which point fields this call operates on: regular
// falloff2 reads/writes tx/ty/tz (accumulating into px/py/pz); post_falloff2
// reads/writes px/py/pz in place (post-transform semantics); pre_falloff2
// reads/writes tx/ty/tz in place (pre-transform semantics). `accumulate`
// selects `+=` (regular) vs `=` (post_/pre_, which always assign).
__device__ inline void devFalloff2Common(DeviceVarContext& c, double inX, double inY, double inZ, bool accumulate,
                                          double& outX, double& outY, double& outZ) {
    const double scatter = c.params[0], mindist = c.params[1];
    const double mulX = c.params[2], mulY = c.params[3], mulZ = c.params[4], mulC = c.params[5];
    const double x0 = c.params[6], y0 = c.params[7], z0 = c.params[8];
    const double invert = c.params[9], mode = c.params[10];
    const double rmax = 0.04 * scatter;

    double px, py, pz;
    if (static_cast<int>(mode) == 1) {
        const double rIn = sqrt(inX * inX + inY * inY + inZ * inZ) + 1e-6;
        const double dist = devFalloffDist(inX - x0, inY - y0, inZ - z0, invert, mindist, rmax);
        const double sigma = asin(inZ / rIn) + mulZ * c.rng->uniform01() * dist;
        const double phi = atan2(inY, inX) + mulY * c.rng->uniform01() * dist;
        const double r = rIn + mulX * c.rng->uniform01() * dist;
        const double sins = sin(sigma), coss = cos(sigma);
        const double sinp = sin(phi), cosp = cos(phi);
        px = c.vvar * (r * coss * cosp);
        py = c.vvar * (r * coss * sinp);
        pz = c.vvar * sins;
        const double colorArg = c.color + mulC * c.rng->uniform01() * dist;
        c.color = fabs(colorArg - trunc(colorArg));
    } else if (static_cast<int>(mode) == 2) {
        const double dist = devFalloffDist(inX - x0, inY - y0, inZ - z0, invert, mindist, rmax);
        const double sigma = dist * c.rng->uniform01() * 2 * kDevicePi;
        const double phi = dist * c.rng->uniform01() * kDevicePi;
        const double r = dist * c.rng->uniform01();
        const double sins = sin(sigma), coss = cos(sigma);
        const double sinp = sin(phi), cosp = cos(phi);
        px = c.vvar * (inX + mulX * r * coss * cosp);
        py = c.vvar * (inY + mulY * r * coss * sinp);
        pz = c.vvar * (inZ + mulZ * r * sins);
        const double colorArg = c.color + mulC * c.rng->uniform01() * dist;
        c.color = fabs(colorArg - trunc(colorArg));
    } else {
        const double dist = devFalloffDist(inX - x0, inY - y0, inZ - z0, invert, mindist, rmax);
        px = c.vvar * (inX + mulX * c.rng->uniform01() * dist);
        py = c.vvar * (inY + mulY * c.rng->uniform01() * dist);
        pz = c.vvar * (inZ + mulZ * c.rng->uniform01() * dist);
        const double colorArg = c.color + mulC * c.rng->uniform01() * dist;
        c.color = fabs(colorArg - trunc(colorArg));
    }

    if (accumulate) {
        outX += px;
        outY += py;
        outZ += pz;
    } else {
        outX = px;
        outY = py;
        outZ = pz;
    }
}

__device__ inline void devFalloff2(DeviceVarContext& c) {
    devFalloff2Common(c, c.tx, c.ty, c.tz, /*accumulate=*/true, c.px, c.py, c.pz);
}
__device__ inline void devPostFalloff2(DeviceVarContext& c) {
    devFalloff2Common(c, c.px, c.py, c.pz, /*accumulate=*/false, c.px, c.py, c.pz);
}
__device__ inline void devPreFalloff2(DeviceVarContext& c) {
    devFalloff2Common(c, c.tx, c.ty, c.tz, /*accumulate=*/false, c.tx, c.ty, c.tz);
}

// Dispatches one variation-step's calc() by device kind id (0..28 local,
// kFirstRegisteredKind+N registered - see VariationKinds.h). Precalc/
// post-transform marker opcodes (kOpPrecalcAngle etc.) are handled directly
// by chaos_kernel.cu's per-op loop, not here - this only ever sees an
// actual variation kind.
__device__ inline void devCalcVariation(int kind, DeviceVarContext& c) {
    switch (kind) {
        case 0: devLinear(c); return;
        case 1: devFlatten(c); return;
        case 2: devSinusoidal(c); return;
        case 3: devSpherical(c); return;
        case 4: devSwirl(c); return;
        case 5: devHorseshoe(c); return;
        case 6: devPolar(c); return;
        case 7: devDisc(c); return;
        case 8: devSpiral(c); return;
        case 9: devHyperbolic(c); return;
        case 10: devDiamond(c); return;
        case 11: devEyefish(c); return;
        case 12: devBubble(c); return;
        case 13: devCylinder(c); return;
        case 14: devNoise(c); return;
        case 15: devBlur(c); return;
        case 16: devGaussianBlur(c); return;
        case 17: devZBlur(c); return;
        case 18: devBlur3D(c); return;
        case 19: devPreBlur(c); return;
        case 20: devPreZScale(c); return;
        case 21: devPreZTranslate(c); return;
        case 22: devPreRotateX(c); return;
        case 23: devPreRotateY(c); return;
        case 24: devZScale(c); return;
        case 25: devZTranslate(c); return;
        case 26: devZCone(c); return;
        case 27: devPostRotateX(c); return;
        case 28: devPostRotateY(c); return;
        case kind::kAuger: devAuger(c); return;
        case kind::kBent: devBent(c); return;
        case kind::kBlob: devBlob(c); return;
        case kind::kCosine: devCosine(c); return;
        case kind::kCross: devCross(c); return;
        case kind::kExponential: devExponential(c); return;
        case kind::kFoci: devFoci(c); return;
        case kind::kHeart: devHeart(c); return;
        case kind::kHemisphere: devHemisphere(c); return;
        case kind::kLazysusan: devLazysusan(c); return;
        case kind::kMobius: devMobius(c); return;
        case kind::kPdj: devPdj(c); return;
        case kind::kPopcorn: devPopcorn(c); return;
        case kind::kPower: devPower(c); return;
        case kind::kPreSinusoidal: devPreSinusoidal(c); return;
        case kind::kPreSpherical: devPreSpherical(c); return;
        case kind::kSecant2: devSecant2(c); return;
        case kind::kSeparation: devSeparation(c); return;
        case kind::kSplits: devSplits(c); return;
        case kind::kTangent: devTangent(c); return;
        case kind::kWaves2: devWaves2(c); return;
        case kind::kBipolar: devBipolar(c); return;
        case kind::kBwraps: devBwraps(c); return;
        case kind::kCurl3D: devCurl3D(c); return;
        case kind::kElliptic: devElliptic(c); return;
        case kind::kEscher: devEscher(c); return;
        case kind::kFan: devFan(c); return;
        case kind::kFan2: devFan2(c); return;
        case kind::kLog: devLog(c); return;
        case kind::kLoonie: devLoonie(c); return;
        case kind::kNGon: devNGon(c); return;
        case kind::kPerspective: devPerspective(c); return;
        case kind::kPolar2: devPolar2(c); return;
        case kind::kPostBwraps: devPostBwraps(c); return;
        case kind::kPostCurl: devPostCurl(c); return;
        case kind::kPostCurl3D: devPostCurl3D(c); return;
        case kind::kPreBwraps: devPreBwraps(c); return;
        case kind::kPreDisc: devPreDisc(c); return;
        case kind::kRings: devRings(c); return;
        case kind::kRings2: devRings2(c); return;
        case kind::kScry: devScry(c); return;
        case kind::kWaves: devWaves(c); return;
        case kind::kWedge: devWedge(c); return;
        case kind::kArch: devArch(c); return;
        case kind::kBlade: devBlade(c); return;
        case kind::kBlurCircle: devBlurCircle(c); return;
        case kind::kBlurPixelize: devBlurPixelize(c); return;
        case kind::kBlurZoom: devBlurZoom(c); return;
        case kind::kCrop: devCrop(c); return;
        case kind::kPostCrop: devPostCrop(c); return;
        case kind::kPreCrop: devPreCrop(c); return;
        case kind::kCurl: devCurl(c); return;
        case kind::kRectangles: devRectangles(c); return;
        case kind::kEpispiral: devEpispiral(c); return;
        case kind::kPie: devPie(c); return;
        case kind::kRays: devRays(c); return;
        case kind::kTwintrian: devTwintrian(c); return;
        case kind::kJulian: devJulian(c); return;
        case kind::kJuliaScope: devJuliaScope(c); return;
        case kind::kJulia3Dz: devJulia3Dz(c); return;
        case kind::kJulia3Djf: devJulia3Djf(c); return;
        case kind::kRadialBlur: devRadialBlur(c); return;
        case kind::kFalloff2: devFalloff2(c); return;
        case kind::kPostFalloff2: devPostFalloff2(c); return;
        case kind::kPreFalloff2: devPreFalloff2(c); return;
        case kind::kSinhPow: devSinhPow(c); return;
        case kind::kSinhPowIA: devSinhPowIA(c); return;
        default: return; // unreachable if RenderDispatcher's eligibility check is correct
    }
}

// Runs xform `xfIdx`'s full per-point op sequence (mirrors XForm::nextPoint,
// see XForm.cpp) - pre_ variations, an optional precalc step, normal
// variations, post_/flatten variations, then an optional post-transform,
// each as one entry of kp.ops[dx.opsOffset .. +dx.opsCount). `x`/`y`/`z`/
// `color` are the running chaos-game point, updated in place.
//
// Color-speed handling matches XForm::nextPoint exactly: colorCoord updates
// by colorC1/colorC2 first, ctx.color (mirrors vc_) starts equal to it and
// is left untouched by every op except the handful of color-aware
// variations (falloff2 family), then the final blend `colorCoord +
// pluginColor * (ctx.color - colorCoord)` folds any such write back in
// scaled by pluginColor - a no-op whenever ctx.color was never touched,
// exactly like the CPU side.
__device__ inline void devXformNextPoint(const DeviceKernelParams& kp, int xfIdx, double& x, double& y, double& z,
                                          double& color, DeviceRng& rng) {
    const DeviceXform& dx = kp.xforms[xfIdx];
    color = color * dx.colorC1 + dx.colorC2;

    DeviceVarContext ctx{};
    ctx.tx = dx.c00 * x + dx.c10 * y + dx.c20;
    ctx.ty = dx.c01 * x + dx.c11 * y + dx.c21;
    ctx.tz = z;
    ctx.px = 0;
    ctx.py = 0;
    ctx.pz = 0;
    ctx.color = color;
    ctx.a = dx.c00;
    ctx.b = dx.c01;
    ctx.c = dx.c10;
    ctx.d = dx.c11;
    ctx.e = dx.c20;
    ctx.f = dx.c21;
    ctx.p00 = dx.p00;
    ctx.p01 = dx.p01;
    ctx.p10 = dx.p10;
    ctx.p11 = dx.p11;
    ctx.p20 = dx.p20;
    ctx.p21 = dx.p21;
    ctx.angle = 0;
    ctx.length = 0;
    ctx.sinA = 0;
    ctx.cosA = 0;
    ctx.rng = &rng;

    const int end = dx.opsOffset + dx.opsCount;
    for (int i = dx.opsOffset; i < end; ++i) {
        const DeviceOp& op = kp.ops[i];
        switch (op.kind) {
            case kOpPrecalcAngle:
                ctx.angle = atan2(ctx.tx, ctx.ty);
                break;
            case kOpPrecalcSinCos:
                ctx.length = sqrt(ctx.tx * ctx.tx + ctx.ty * ctx.ty) + kDeviceEps;
                ctx.sinA = ctx.tx / ctx.length;
                ctx.cosA = ctx.ty / ctx.length;
                break;
            case kOpPrecalcAll:
                ctx.length = sqrt(ctx.tx * ctx.tx + ctx.ty * ctx.ty) + kDeviceEps;
                ctx.sinA = ctx.tx / ctx.length;
                ctx.cosA = ctx.ty / ctx.length;
                ctx.angle = atan2(ctx.tx, ctx.ty);
                break;
            case kOpPostTransform: {
                const double tmp = ctx.px;
                ctx.px = ctx.p00 * ctx.px + ctx.p10 * ctx.py + ctx.p20;
                ctx.py = ctx.p01 * tmp + ctx.p11 * ctx.py + ctx.p21;
                break;
            }
            default:
                ctx.vvar = op.vvar;
                ctx.params = kp.varParams + op.paramOffset;
                devCalcVariation(op.kind, ctx);
                break;
        }
    }

    x = ctx.px;
    y = ctx.py;
    z = ctx.pz;
    color = color + dx.pluginColor * (ctx.color - color);
}

} // namespace apo::gpu
