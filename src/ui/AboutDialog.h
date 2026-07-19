#pragma once

#include <QDialog>

// Port of About.pas (119 lines) - version/credits dialog. The original's
// credited third-party dependencies (TMS Scripter Studio, a PCRE-for-Delphi
// regex library, a custom XML parser) are all specific to the Delphi build
// and don't apply here - see AboutDialog.cpp for what replaces them (Qt6,
// pugixml, libpng), plus continued attribution to flam3/the original
// Apophysis authors for the rendering algorithms and variation library
// this port's math is still directly based on.
namespace apo::ui {

class AboutDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace apo::ui
