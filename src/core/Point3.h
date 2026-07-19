#pragma once

namespace apo {

// Mirrors Delphi's TCPpoint (Flame/XForm.pas): a chaos-game point plus its
// color coordinate. Kept as a plain struct - the render hot path (Phase 3)
// passes these by value/reference through millions of iterations per second.
struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

} // namespace apo
