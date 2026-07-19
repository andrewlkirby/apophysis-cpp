#include "FlameIO.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <sstream>

#include <pugixml.hpp>

#include "../Bezier.h"
#include "../VariationRegistry.h"

namespace apo {
namespace {

// Fallback defaults for attributes absent from the XML, matching
// Core/Global.pas's def* globals (verified directly against source - these
// are the hardcoded literals Settings.pas/Regstry.pas fall back to when no
// saved user preference exists, which is also exactly what
// ParameterIO.pas's GetFloatPart(...) calls use as their own default
// argument for these specific attributes).
constexpr double kDefGamma = 4.0;
constexpr double kDefBrightness = 4.0;
constexpr double kDefVibrancy = 1.0;
constexpr double kDefFilterRadius = 0.2;
constexpr int kDefOversample = 1;
constexpr double kDefGammaThreshold = 0.01;
constexpr double kDefScale = 25.0;      // "scale" (pixelsPerUnit) attribute default
constexpr double kDefQuality = 5.0;     // "quality" (sampleDensity) attribute default

double attrFloat(pugi::xml_node node, const char* name, double def) {
    pugi::xml_attribute a = node.attribute(name);
    return a ? a.as_double(def) : def;
}

int attrInt(pugi::xml_node node, const char* name, int def) {
    pugi::xml_attribute a = node.attribute(name);
    return a ? a.as_int(def) : def;
}

bool attrBool(pugi::xml_node node, const char* name, bool def) {
    pugi::xml_attribute a = node.attribute(name);
    return a ? a.as_bool(def) : def;
}

std::string attrString(pugi::xml_node node, const char* name, const char* def) {
    pugi::xml_attribute a = node.attribute(name);
    return a ? std::string(a.as_string(def)) : std::string(def);
}

// "X Y" -> two doubles.
std::array<double, 2> attr2Float(pugi::xml_node node, const char* name, double def) {
    std::array<double, 2> result{def, def};
    pugi::xml_attribute a = node.attribute(name);
    if (!a) return result;
    std::istringstream iss(a.as_string());
    double x, y;
    if (iss >> x >> y) result = {x, y};
    return result;
}

// "W H" -> two ints.
void attr2Int(pugi::xml_node node, const char* name, int& outW, int& outH) {
    pugi::xml_attribute a = node.attribute(name);
    if (!a) return;
    std::istringstream iss(a.as_string());
    int w, h;
    if (iss >> w >> h) {
        outW = w;
        outH = h;
    }
}

// "R G B" -> three ints in [0,255]. Unlike the original (which *saves*
// background as a 0-1 fraction via "%g" but *loads* it via an
// integer-only regex "(\d+)\s+(\d+)\s+(\d+)" - confirmed directly against
// RegexHelper.pas's GetRGBPart - so any non-integer-valued background,
// which is what its own save path always produces unless the color is a
// pure 0/255 extreme, silently fails to parse and falls back to black on
// reload), this port uses one consistent integer 0-255 encoding on both
// sides - see writeFlameAttributes() below. Fixed, not replicated: this
// is a real round-trip data-loss bug in the original, not a deliberate
// design choice worth preserving.
std::array<int, 3> attrRgb(pugi::xml_node node, const char* name, int def) {
    std::array<int, 3> result{def, def, def};
    pugi::xml_attribute a = node.attribute(name);
    if (!a) return result;
    std::istringstream iss(a.as_string());
    int r, g, b;
    if (iss >> r >> g >> b) result = {r, g, b};
    return result;
}

// "a d b e c f" (six floats) -> the 3x2 affine coefficient array, matching
// XForm.h's c[row][col] layout (c[0]=(a,d), c[1]=(b,e), c[2]=(c,f) - see
// its own doc comment) exactly, verified directly against
// ParameterIO.pas's coefs-parsing order.
void parseCoefs(const std::string& text, std::array<std::array<double, 2>, 3>& c) {
    std::istringstream iss(text);
    double a, d, b, e, cc, f;
    if (iss >> a >> d >> b >> e >> cc >> f) {
        c[0] = {a, d};
        c[1] = {b, e};
        c[2] = {cc, f};
    }
}

std::string formatCoefs(const std::array<std::array<double, 2>, 3>& c) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%g %g %g %g %g %g", c[0][0], c[0][1], c[1][0], c[1][1], c[2][0], c[2][1]);
    return buf;
}

// "P0.x P0.y W0 P1.x P1.y W1 P2.x P2.y W2 P3.x P3.y W3" x4 channels (48
// floats total) -> the 4 BezierCurve channels (Curves dialog tone
// curves), matching ParameterIO.pas's `curves` attribute parsing order
// exactly: channel-major (All, Red, Green, Blue), then point-major within
// a channel, then x/y/weight within a point.
void parseCurves(const std::string& text, std::array<BezierCurve, 4>& curves) {
    std::istringstream iss(text);
    for (auto& curve : curves) {
        for (int p = 0; p < 4; ++p) {
            double x, y, w;
            if (!(iss >> x >> y >> w)) return; // malformed/truncated - leave the remainder at their defaults
            curve.points[static_cast<size_t>(p)] = {x, y};
            curve.weights[static_cast<size_t>(p)] = w;
        }
    }
}

std::string formatCurves(const std::array<BezierCurve, 4>& curves) {
    std::string out;
    char buf[128];
    for (const auto& curve : curves) {
        for (int p = 0; p < 4; ++p) {
            std::snprintf(buf, sizeof(buf), "%g %g %g ", curve.points[static_cast<size_t>(p)][0],
                          curve.points[static_cast<size_t>(p)][1], curve.weights[static_cast<size_t>(p)]);
            out += buf;
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Matches LoadXFormFromXmlCompatible exactly (verified directly against
// ParameterIO.pas), including: zeroing every variation weight before
// applying the file's attributes (NOT starting from XForm::clear()'s
// linear=1 default - a saved xform that doesn't mention "linear" at all
// means linear's weight really is 0, not implicitly 1), rejecting
// weight/symmetry/color_speed/chaos/opacity/name/plotmode on a final
// xform, and forcing symmetry=1/color=0 on a final xform after parsing
// (unconditionally, even if a stray `color` attribute was present - the
// original does exactly this, not a port-introduced restriction).
void parseXForm(pugi::xml_node node, XForm& xf, bool isFinal) {
    xf.clear();
    for (int i = 0; i < xf.numVariations(); ++i) xf.setVariation(i, 0.0);

    auto& registry = VariationRegistry::instance();

    for (pugi::xml_attribute attr : node.attributes()) {
        const std::string name = attr.name();

        if (name == "weight" && !isFinal) {
            xf.density = attr.as_double(0.5);
        } else if ((name == "symmetry" || name == "color_speed") && !isFinal) {
            xf.symmetry = attr.as_double(0.0);
        } else if (name == "chaos" && !isFinal) {
            std::istringstream iss(attr.as_string());
            double w;
            int i = 0;
            while (iss >> w && i < static_cast<int>(xf.modWeights.size())) {
                xf.modWeights[i++] = std::fabs(w);
            }
        } else if ((name == "opacity" || name == "plotmode") && !isFinal) {
            if (name == "plotmode") {
                std::string v = attr.as_string();
                for (char& ch : v) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                xf.transOpacity = (v == "off") ? 0.0 : 1.0;
            } else {
                xf.transOpacity = attr.as_double(1.0);
            }
        } else if (name == "name" && !isFinal) {
            xf.transformName = attr.as_string();
        } else if (name == "coefs") {
            parseCoefs(attr.as_string(), xf.c);
        } else if (name == "post") {
            parseCoefs(attr.as_string(), xf.p);
        } else if (name == "color") {
            xf.color = attr.as_double(0.0);
        } else if (name == "var_color") {
            xf.pluginColor = attr.as_double(1.0);
        } else if (name == "linear3D") {
            xf.setVariation(0, attr.as_double(0.0));
        } else if (name == "enabled" || name == "final") {
            // Final-xform "enabled" is parsed-but-discarded by the original
            // too (LoadXFormFromXmlCompatible reads it into a throwaway
            // `dummy` local it never applies to cp.useFinalXform, and the
            // save side's own comment says the flag is "disabled in this
            // release" - it's vestigial in both directions). Skip.
        } else if (isFinal && (name == "symmetry" || name == "weight" || name == "color_speed" ||
                                name == "chaos" || name == "opacity" || name == "name" || name == "plotmode")) {
            // Not valid on a final xform - ignored, matching the original.
        } else {
            const int idx = registry.variationIndex(name);
            if (idx >= 0) {
                xf.setVariation(idx, attr.as_double(0.0));
            } else {
                double value = attr.as_double(0.0);
                xf.setVariable(name, value); // no-op if `name` isn't a known parameter
            }
        }
    }

    if (isFinal) {
        xf.symmetry = 1;
        xf.color = 0;
    }
}

void writeXFormAttributes(pugi::xml_node node, const XForm& xf, bool isFinal) {
    if (!isFinal) {
        node.append_attribute("weight") = xf.density;
        if (xf.symmetry != 0) node.append_attribute("color_speed") = xf.symmetry;
        node.append_attribute("opacity") = xf.transOpacity;
        if (!xf.transformName.empty()) node.append_attribute("name") = xf.transformName.c_str();

        bool anyNonDefaultChaos = false;
        for (int i = 0; i < static_cast<int>(xf.modWeights.size()); ++i) {
            if (xf.modWeights[i] != 1.0) {
                anyNonDefaultChaos = true;
                break;
            }
        }
        if (anyNonDefaultChaos) {
            std::ostringstream oss;
            for (int i = 0; i < static_cast<int>(xf.modWeights.size()); ++i) {
                if (i > 0) oss << ' ';
                oss << xf.modWeights[i];
            }
            node.append_attribute("chaos") = oss.str().c_str();
        }
    }

    node.append_attribute("color") = xf.color;
    node.append_attribute("var_color") = xf.pluginColor;
    node.append_attribute("coefs") = formatCoefs(xf.c).c_str();

    const std::array<std::array<double, 2>, 3> identity{{{1, 0}, {0, 1}, {0, 0}}};
    if (xf.p != identity) {
        node.append_attribute("post") = formatCoefs(xf.p).c_str();
    }

    auto& registry = VariationRegistry::instance();
    for (int i = 0; i < xf.numVariations(); ++i) {
        const double w = xf.variation(i);
        if (w == 0) continue;
        node.append_attribute(registry.varName(i).c_str()) = w;

        if (i >= VariationRegistry::kNumLocalVars) {
            const auto& factory = registry.registeredVariation(i - VariationRegistry::kNumLocalVars);
            const int n = factory.numVariables();
            for (int j = 0; j < n; ++j) {
                const std::string paramName = factory.variableNameAt(j);
                double value = 0;
                if (xf.getVariable(paramName, value)) {
                    node.append_attribute(paramName.c_str()) = value;
                }
            }
        }
    }
}

// Matches ColorToXmlCompact/the inline <palette> loader exactly (verified
// directly against ParameterIO.pas): a flat run of 256 6-hex-digit RGB
// triplets. Whitespace/newlines between triplets are ignored on load (the
// writer wraps every 8 entries purely for readability, matching the
// original's own formatting, but this isn't load-significant).
void parsePalette(pugi::xml_node node, ColorMap& cmap) {
    std::string hex;
    hex.reserve(1536);
    for (char c : std::string(node.text().as_string())) {
        if (std::isxdigit(static_cast<unsigned char>(c))) hex.push_back(c);
    }

    const int count = std::min<int>(256, static_cast<int>(hex.size() / 6));
    for (int i = 0; i < count; ++i) {
        unsigned int rgb = 0;
        const char* start = hex.c_str() + static_cast<size_t>(i) * 6;
        std::from_chars(start, start + 6, rgb, 16);
        cmap.entries[i][0] = static_cast<int>((rgb >> 16) & 0xFF);
        cmap.entries[i][1] = static_cast<int>((rgb >> 8) & 0xFF);
        cmap.entries[i][2] = static_cast<int>(rgb & 0xFF);
        cmap.entries[i][3] = 255;
    }
}

void writePalette(pugi::xml_node flameNode, const ColorMap& cmap) {
    pugi::xml_node palette = flameNode.append_child("palette");
    palette.append_attribute("count") = 256;
    palette.append_attribute("format") = "RGB";

    std::ostringstream oss;
    for (int i = 0; i < 256; ++i) {
        if (i % 8 == 0) oss << "\n      ";
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02x%02x%02x", cmap.entries[i][0] & 0xFF, cmap.entries[i][1] & 0xFF,
                      cmap.entries[i][2] & 0xFF);
        oss << buf;
    }
    oss << "\n   ";
    palette.append_child(pugi::node_pcdata).set_value(oss.str().c_str());
}

// Matches LoadCpFromXmlCompatible exactly (verified directly against
// ParameterIO.pas) for every attribute this port's Flame models. `angle`
// and `rotate` both load into Flame::angle - the original has a confirmed
// bug where `rotate` is written into `vibrancy` instead (a copy-paste
// error; see Flame.h's `angle` field comment) - fixed here, not
// reproduced.
std::unique_ptr<Flame> parseFlame(pugi::xml_node flameNode) {
    auto flame = std::make_unique<Flame>();
    flame->clear();

    flame->name = attrString(flameNode, "name", "");
    attr2Int(flameNode, "size", flame->width, flame->height);
    flame->center = attr2Float(flameNode, "center", 0.0);
    flame->pixelsPerUnit = attrFloat(flameNode, "scale", kDefScale);
    flame->sampleDensity = attrFloat(flameNode, "quality", kDefQuality);
    flame->zoom = attrFloat(flameNode, "zoom", 0.0);

    flame->vibrancy = attrFloat(flameNode, "vibrancy", kDefVibrancy);
    flame->brightness = attrFloat(flameNode, "brightness", kDefBrightness);
    flame->gamma = attrFloat(flameNode, "gamma", kDefGamma);
    flame->gammaThreshold = attrFloat(flameNode, "gamma_threshold", kDefGammaThreshold);
    flame->spatialOversample = attrInt(flameNode, "oversample", kDefOversample);
    flame->spatialFilterRadius = attrFloat(flameNode, "filter", kDefFilterRadius);

    if (flameNode.attribute("angle")) {
        flame->angle = attrFloat(flameNode, "angle", 0.0);
    } else if (flameNode.attribute("rotate")) {
        flame->angle = -3.14159265358979323846 * attrFloat(flameNode, "rotate", 0.0) / 180.0;
    }

    flame->cameraPitch = attrFloat(flameNode, "cam_pitch", 0.0);
    flame->cameraYaw = attrFloat(flameNode, "cam_yaw", 0.0);
    flame->cameraPersp = attrFloat(flameNode, "cam_perspective", 1.0);
    if (flameNode.attribute("cam_dist")) {
        double d = attrFloat(flameNode, "cam_dist", 1.0);
        if (d == 0) d = 1e-10;
        flame->cameraPersp = 1.0 / d;
    }
    flame->cameraZpos = attrFloat(flameNode, "cam_zpos", 0.0);
    flame->cameraDOF = attrFloat(flameNode, "cam_dof", 0.0);

    flame->estimator = attrFloat(flameNode, "estimator_radius", 0.0);
    flame->estimatorMin = attrFloat(flameNode, "estimator_minimum", 0.0);
    flame->estimatorCurve = attrFloat(flameNode, "estimator_curve", 0.0);
    flame->enableDE = attrBool(flameNode, "enable_de", false);

    const auto bg = attrRgb(flameNode, "background", 0);
    flame->background[0] = bg[0];
    flame->background[1] = bg[1];
    flame->background[2] = bg[2];

    flame->nbatches = attrInt(flameNode, "batches", 1);
    flame->time = attrFloat(flameNode, "time", 0.0);
    flame->soloXform = flameNode.attribute("soloxform") ? attrInt(flameNode, "soloxform", -1) : -1;

    // flame->clear() above already leaves flame->curves at the default
    // identity configuration - only overwrite it if the file actually
    // carries a `curves` attribute (matches every other optional
    // attribute's absent-means-default contract here).
    if (flameNode.attribute("curves")) {
        parseCurves(attrString(flameNode, "curves", ""), flame->curves);
    }

    for (auto& x : flame->xform) x->density = 0;

    int xformIndex = 0;
    for (pugi::xml_node child : flameNode.children()) {
        const std::string tag = child.name();
        if (tag == "xform" && xformIndex < Flame::kMaxXForms) {
            parseXForm(child, *flame->xform[xformIndex], false);
            ++xformIndex;
        } else if (tag == "finalxform") {
            parseXForm(child, *flame->finalXform, true);
            flame->finalXformEnabled = true;
            flame->useFinalXform = true;
        } else if (tag == "palette") {
            parsePalette(child, flame->cmap);
        }
    }

    return flame;
}

void writeFlame(pugi::xml_node parent, const Flame& flame) {
    pugi::xml_node node = parent.append_child("flame");
    node.append_attribute("name") = flame.name.c_str();
    node.append_attribute("version") = "Apophysis 7X (C++ port)";
    if (flame.time != 0) node.append_attribute("time") = flame.time;

    char sizeBuf[32];
    std::snprintf(sizeBuf, sizeof(sizeBuf), "%d %d", flame.width, flame.height);
    node.append_attribute("size") = sizeBuf;
    node.append_attribute("center") = (std::to_string(flame.center[0]) + " " + std::to_string(flame.center[1])).c_str();
    node.append_attribute("scale") = flame.pixelsPerUnit;
    node.append_attribute("quality") = flame.sampleDensity;

    if (flame.angle != 0) {
        node.append_attribute("angle") = flame.angle;
        node.append_attribute("rotate") = -180.0 * flame.angle / 3.14159265358979323846;
    }
    if (flame.zoom != 0) node.append_attribute("zoom") = flame.zoom;

    if (flame.cameraPitch != 0) node.append_attribute("cam_pitch") = flame.cameraPitch;
    if (flame.cameraYaw != 0) node.append_attribute("cam_yaw") = flame.cameraYaw;
    if (flame.cameraPersp != 0) node.append_attribute("cam_perspective") = flame.cameraPersp;
    if (flame.cameraZpos != 0) node.append_attribute("cam_zpos") = flame.cameraZpos;
    if (flame.cameraDOF != 0) node.append_attribute("cam_dof") = flame.cameraDOF;

    node.append_attribute("oversample") = flame.spatialOversample;
    node.append_attribute("filter") = flame.spatialFilterRadius;
    node.append_attribute("vibrancy") = flame.vibrancy;
    node.append_attribute("gamma") = flame.gamma;
    node.append_attribute("gamma_threshold") = flame.gammaThreshold;
    node.append_attribute("brightness") = flame.brightness;
    // Fixed encoding (see attrRgb's comment): plain 0-255 integers,
    // matching what the loader actually parses, instead of the original's
    // write-0-1-fraction/read-integers-only mismatch.
    char bgBuf[32];
    std::snprintf(bgBuf, sizeof(bgBuf), "%d %d %d", flame.background[0], flame.background[1], flame.background[2]);
    node.append_attribute("background") = bgBuf;

    if (flame.estimator != 0) node.append_attribute("estimator_radius") = flame.estimator;
    if (flame.estimatorMin != 0) node.append_attribute("estimator_minimum") = flame.estimatorMin;
    if (flame.estimatorCurve != 0) node.append_attribute("estimator_curve") = flame.estimatorCurve;
    if (flame.enableDE) node.append_attribute("enable_de") = 1;

    if (flame.nbatches != 1) node.append_attribute("batches") = flame.nbatches;
    if (flame.soloXform >= 0) node.append_attribute("soloxform") = flame.soloXform;

    // Only written for a flame that actually has non-default curves - keeps
    // a saved file's `curves` attribute (48 floats) out of the common case
    // where the Curves dialog was never touched.
    if (flame.curves != std::array<BezierCurve, 4>{}) {
        node.append_attribute("curves") = formatCurves(flame.curves).c_str();
    }

    const int n = flame.numXForms();
    for (int i = 0; i < n; ++i) {
        pugi::xml_node xformNode = node.append_child("xform");
        writeXFormAttributes(xformNode, *flame.xform[i], false);
    }
    if (flame.finalXformEnabled) {
        pugi::xml_node finalNode = node.append_child("finalxform");
        writeXFormAttributes(finalNode, *flame.finalXform, true);
    }

    writePalette(node, flame.cmap);
}

std::vector<std::unique_ptr<Flame>> parseDocument(const pugi::xml_document& doc) {
    std::vector<std::unique_ptr<Flame>> result;

    pugi::xml_node root = doc.document_element();
    if (!root) return result;

    const std::string rootName = root.name();
    if (rootName == "flame") {
        result.push_back(parseFlame(root));
    } else if (rootName == "flames") {
        for (pugi::xml_node child : root.children("flame")) {
            result.push_back(parseFlame(child));
        }
    }
    return result;
}

void buildDocument(pugi::xml_document& doc, const std::vector<const Flame*>& flames) {
    if (flames.size() == 1) {
        writeFlame(doc, *flames[0]);
    } else {
        pugi::xml_node root = doc.append_child("flames");
        root.append_attribute("name") = "flames";
        for (const Flame* f : flames) writeFlame(root, *f);
    }
}

} // namespace

std::vector<std::unique_ptr<Flame>> loadFlameFile(const std::string& path) {
    pugi::xml_document doc;
    if (!doc.load_file(path.c_str())) return {};
    return parseDocument(doc);
}

bool saveFlameFile(const std::string& path, const std::vector<const Flame*>& flames) {
    if (flames.empty()) return false;
    pugi::xml_document doc;
    buildDocument(doc, flames);
    return doc.save_file(path.c_str(), "   ");
}

std::vector<std::unique_ptr<Flame>> loadFlameFromString(const std::string& xml) {
    pugi::xml_document doc;
    if (!doc.load_string(xml.c_str())) return {};
    return parseDocument(doc);
}

std::string saveFlameToString(const std::vector<const Flame*>& flames) {
    if (flames.empty()) return {};
    pugi::xml_document doc;
    buildDocument(doc, flames);
    std::ostringstream oss;
    doc.save(oss, "   ");
    return oss.str();
}

} // namespace apo
