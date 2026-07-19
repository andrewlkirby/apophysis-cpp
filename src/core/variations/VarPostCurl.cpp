#include "VarPostCurl.h"

#include "../VariationRegistration.h"

namespace apo {

void VarPostCurl::prepare() {
    // See the class comment in VarPostCurl.h: derive into separate cached
    // locals instead of mutating the stored c1_/c2_ parameters in place.
    c1Scaled_ = c1_ * vvar;
    c2Scaled_ = c2_ * vvar;
    c22_ = 2 * c2Scaled_;
}

void VarPostCurl::calc() {
    const double x = *px;
    const double y = *py;

    const double re = 1 + c1Scaled_ * x + c2Scaled_ * (x * x - y * y);
    const double im = c1Scaled_ * y + c22_ * x * y;

    const double r = re * re + im * im;
    *px = (x * re + y * im) / r;
    *py = (y * re - x * im) / r;
}

std::string VarPostCurl::variableNameAt(int index) const {
    switch (index) {
        case 0: return "post_curl_c1";
        case 1: return "post_curl_c2";
        default: return "";
    }
}

bool VarPostCurl::getVariable(const std::string& name, double& value) const {
    if (name == "post_curl_c1") { value = c1_; return true; }
    if (name == "post_curl_c2") { value = c2_; return true; }
    return false;
}

bool VarPostCurl::setVariable(const std::string& name, double& value) {
    if (name == "post_curl_c1") { c1_ = value; return true; }
    if (name == "post_curl_c2") { c2_ = value; return true; }
    return false;
}

bool VarPostCurl::resetVariable(const std::string& name) {
    if (name == "post_curl_c1") { c1_ = 0; return true; }
    if (name == "post_curl_c2") { c2_ = 0; return true; }
    return false;
}

namespace {
const bool kRegistered = registerVariation<VarPostCurl>();
}

} // namespace apo
