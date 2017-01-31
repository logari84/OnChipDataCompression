/*! The class that applies different encoding schemas to the chip data.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "Chip.h"
#include "AlphabetStatisticsCollection.h"
#include "PackageMaker.h"

namespace pixel_studies {

enum class EncoderFormat { SinglePixel, Region, RegionWithCompressedAdc, Delta };

class ChipDataEncoder {
public:
    using Letter = int;
    using StatisticsSource = AlphabetStatisticsCollection<Letter>;

    ChipDataEncoder(EncoderFormat encoder_format, const MultiRegionLayout& _chip_layout,
                    const RegionLayout& readout_unit_layout, size_t max_adc,
                    Ordering ordering = Ordering::ByRegionByColumn, const std::string& dictionary_file = "");
    Package Encode(const Chip& chip) const;
    Chip Decode(const Package& package) const;

private:
    const MultiRegionLayout chip_layout;
    std::shared_ptr<PackageMaker> package_maker;
    std::shared_ptr<StatisticsSource> statistics_source;
};

} // namespace onchip_algorithms
