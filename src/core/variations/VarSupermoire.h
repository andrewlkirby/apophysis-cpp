#pragma once

#include "../Variation.h"

namespace apo {

// A novel (not ported from Delphi/flam3) variation inspired by Ning & Gedik,
// "Visualizing the moire of moire", Nat. Mater. 23, 1606-1607 (2024): a
// "supermoire" second-order interference pattern emerges when two triangular
// lattice potentials of nearly equal periodicity, independently twisted and
// with a small uniaxial heterostrain applied to one of them, are combined
// nonlinearly (a linear sum alone reproduces only a triangular lattice; the
// paper's model needs the nonlinear term to match the observed bidirectional
// fringes and the triangular -> quasi-square -> one-dimensional transition
// driven by strain, Fig. 1b-d).
class VarSupermoire final : public Variation {
public:
    static constexpr const char* kName = "supermoire";
    static constexpr bool kSupports3D = false;
    static constexpr bool kSupportsDC = false;

    void calc() override;

    int numVariables() const override { return 7; }
    std::string variableNameAt(int index) const override;
    bool getVariable(const std::string& name, double& value) const override;
    bool setVariable(const std::string& name, double& value) override;
    bool resetVariable(const std::string& name) override;

private:
    // Sum of three plane waves at 120 degrees apart - a triangular lattice
    // potential of the given spatial frequency and twist angle (radians).
    static double triLattice(double x, double y, double angle, double freq);
    // The nonlinear supermoire field: two triLattice() potentials (the
    // second sampled through a uniaxial-strain-warped coordinate) combined
    // as L1 + L2 + mix*L1*L2.
    double field(double x, double y) const;

    double freq1_ = 8, freq2_ = 9;
    double angle1_ = 0, angle2_ = 0.12;
    double strain_ = 0;
    double mix_ = 0.5;
    double scale_ = 0.2;
};

} // namespace apo
