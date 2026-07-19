#include "PluginVariation.h"

#include "../VariationRegistry.h"

namespace apo {

PluginVariation::PluginVariation(const PluginVTable& vt) : vt_(vt), state_(vt.create()) {}

PluginVariation::~PluginVariation() { vt_.destroy(&state_); }

void PluginVariation::prepare() {
    // Rebind the plugin's own opaque state to this XForm::prepare() call's
    // pointers/coefficients every time, matching Variation::tx/ty/.../vvar
    // being (re)bound by XForm::prepare() before prepare()/calc() run.
    vt_.initDC(state_, px, py, pz, tx, ty, tz, color, vvar, a, b, c, d, e, f);
    vt_.prepare(state_);
}

void PluginVariation::calc() { vt_.calc(state_); }

bool PluginVariation::getVariable(const std::string& name, double& value) const {
    return vt_.getVariable(state_, name.c_str(), &value) != 0;
}

bool PluginVariation::setVariable(const std::string& name, double& value) {
    return vt_.setVariable(state_, name.c_str(), &value) != 0;
}

bool PluginVariation::resetVariable(const std::string& name) {
    return vt_.resetVariable(state_, name.c_str()) != 0;
}

PluginVariationFactory::PluginVariationFactory(std::string name, const PluginVTable& vt)
    : name_(std::move(name)), vt_(vt) {}

bool registerPluginVariation(std::string name, const PluginVTable& vt) {
    auto factory = std::make_unique<PluginVariationFactory>(std::move(name), vt);
    // See PluginVTable::initDC's comment: every plugin is always driven
    // through the full 3D/DirectColor-capable path, so both are reported
    // as supported regardless of whether a given plugin actually uses
    // z/color. Harmless for rendering (unused fields are simply never
    // read); revisit only if a future UI needs per-plugin precision here.
    factory->supports3D = true;
    factory->supportsDC = true;
    VariationRegistry::instance().registerVariation(std::move(factory));
    return true;
}

} // namespace apo
