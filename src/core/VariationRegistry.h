#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Variation.h"

namespace apo {

// Mirrors Delphi's XFormMan.pas (Core/XFormMan.pas): the global table of
// registered variation types, plus the flattened "variable name -> owning
// variation index" lookup the editor UI uses to build its variable list.
//
// NRLOCVAR (29) built-in "local" variations are hardcoded here to match
// XForm's own local variation dispatch table 1:1 (see XForm::kLocalVarNames)
// - they're not registered through this registry in Delphi either; XFormMan
// just knows their count and names so NrVar()/Varnames() can present a
// single combined index space (0..NRLOCVAR-1 local, NRLOCVAR.. registered).
//
// Phase 2 populates this registry with the 48 built-in Variations/*.pas
// ports plus the 55 statically-linked Plugin/*.c wrappers. Empty for now.
class VariationRegistry {
public:
    static constexpr int kNumLocalVars = 29; // NRLOCVAR

    static VariationRegistry& instance();

    // Local variation names, index 0..kNumLocalVars-1, matching XFormMan's
    // cvarnames exactly (including the 'diamond' / Square naming quirk).
    static const std::array<std::string, kNumLocalVars>& localVarNames();

    void registerVariation(std::unique_ptr<VariationFactory> factory);

    int numRegisteredVariations() const { return static_cast<int>(factories_.size()); }
    const VariationFactory& registeredVariation(int index) const { return *factories_.at(index); }

    // Combined index space: 0..kNumLocalVars-1 are local, the rest are
    // registered variations. Matches XFormMan.NrVar()/Varnames().
    int nrVar() const { return kNumLocalVars + numRegisteredVariations(); }
    std::string varName(int index) const;
    int variationIndex(const std::string& name) const;

    // Flattened per-variable-name lookup (XFormMan.GetVariableNameAt /
    // GetVariationIndexFromVariableNameIndex), used by the editor's variable
    // panel to enumerate every settable parameter across every registered
    // variation.
    int numVariableNames() const { return static_cast<int>(variableNames_.size()); }
    const std::string& variableNameAt(int index) const { return variableNames_.at(index); }
    int variationIndexFromVariableNameIndex(int index) const;

private:
    VariationRegistry() = default;

    std::vector<std::unique_ptr<VariationFactory>> factories_;
    std::vector<std::string> variableNames_;
    std::vector<int> variableNameToVariationIndex_; // parallel to variableNames_
};

} // namespace apo
