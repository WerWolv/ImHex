#pragma once

#include <hex/providers/provider.hpp>

namespace pl::ptrn { class Pattern; }
namespace hex::prv { class Provider; }

namespace hex::ui {


    enum class RegionType : int {
        EntireData,
        Selection,
        Region
    };

    void regionSelectionPicker(Region *region, prv::Provider *provider, RegionType *type, bool showHeader = true, bool firstEntry = false);
    bool endiannessSlider(std::endian &endian);

}