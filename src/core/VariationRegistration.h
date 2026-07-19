#pragma once

#include <memory>
#include <string>

#include "Variation.h"
#include "VariationRegistry.h"

namespace apo {

// Generic VariationFactory for the common case: a Variation subclass that
// exposes its name and 3D/DC support as static members (kName,
// kSupports3D, kSupportsDC). Answers numVariables()/variableNameAt() by
// building a throwaway instance and asking it - matches Delphi's
// TVariationClassLoader (Core/BaseVariation.pas), which does exactly the
// same "make a throwaway instance to answer these questions" trick rather
// than duplicating the answers in a separate loader/factory type per
// variation:
//
//   function TVariationClassLoader.GetNrVariables: integer;
//   var hack : TBaseVariation;
//   begin
//     hack := GetInstance();
//     Result := hack.GetNrVariables();
//     hack.Free();
//   end;
//
// This means an individual Variations/Var*.h file only needs to declare the
// Variation subclass itself - no separate per-file factory class - keeping
// each of the ~46 ports to one class instead of two.
template <typename VariationT>
class SimpleVariationFactory final : public VariationFactory {
public:
    std::string name() const override { return VariationT::kName; }
    std::unique_ptr<Variation> create() const override { return std::make_unique<VariationT>(); }
    int numVariables() const override { return VariationT().numVariables(); }
    std::string variableNameAt(int index) const override { return VariationT().variableNameAt(index); }
};

// Registers VariationT with the global VariationRegistry. Call once, from a
// file-scope static initializer in each variation's .cpp:
//
//   namespace { const bool kRegistered = apo::registerVariation<VarAuger>(); }
//
// matching Delphi's `initialization RegisterVariation(...)` at the end of
// each var*.pas unit. Safe regardless of static-init order across
// translation units: VariationRegistry::instance() is a function-local
// (Meyer's) singleton, so it's constructed on first use by whichever
// registerVariation<T>() call happens to run first.
template <typename VariationT>
bool registerVariation() {
    auto factory = std::make_unique<SimpleVariationFactory<VariationT>>();
    factory->supports3D = VariationT::kSupports3D;
    factory->supportsDC = VariationT::kSupportsDC;
    VariationRegistry::instance().registerVariation(std::move(factory));
    return true;
}

} // namespace apo
