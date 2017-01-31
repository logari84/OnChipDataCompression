/*! The class that applies different encoding schemas to the chip data.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#include <fstream>
#include "../interface/ChipDataEncoder.h"
#include "../interface/BlockPackageMaker.h"
#include "../interface/DeltaPackageMaker.h"
#include "../interface/HuffmanDecoder.h"
#include "../interface/HuffmanEncoder.h"

namespace pixel_studies {

ChipDataEncoder::ChipDataEncoder(EncoderFormat encoder_format, const MultiRegionLayout& _chip_layout,
                                 const RegionLayout& readout_unit_layout, size_t max_adc,
                                 Ordering ordering, const std::string& dictionary_file) :
    chip_layout(_chip_layout)
{
    using BlockPackageMaker = BlockPackageMaker<HuffmanDecoder>;
    using DeltaPackageMaker = DeltaPackageMaker<HuffmanDecoder>;

    const size_t n_bits_per_adc = RegionLayout::BitsPerValue(max_adc);
    if(encoder_format == EncoderFormat::SinglePixel) {
        package_maker = std::make_shared<DefaultPackageMaker>(n_bits_per_adc);
    } else if(encoder_format == EncoderFormat::Region) {
        package_maker = std::make_shared<BlockPackageMaker>(nullptr, readout_unit_layout, n_bits_per_adc, false);
    } else if(encoder_format == EncoderFormat::RegionWithCompressedAdc || encoder_format == EncoderFormat::Delta) {
        statistics_source = std::make_shared<StatisticsSource>(dictionary_file);
        if(encoder_format == EncoderFormat::RegionWithCompressedAdc) {
            package_maker = std::make_shared<BlockPackageMaker>(statistics_source.get(), readout_unit_layout,
                                                                n_bits_per_adc, true);
        } else {
            package_maker = std::make_shared<DeltaPackageMaker>(*statistics_source, readout_unit_layout,
                                                                DeltaPackageMakerMode::CombinedDelta, ordering);
        }
    } else {
        throw exception("Encoder format is not supported.");
    }
}

Package ChipDataEncoder::Encode(const Chip& original_chip) const
{
    ChipPtr split_chip;
    const Chip* chip = nullptr;
    if(original_chip.GetMultiRegionLayout() == chip_layout) {
        chip = &original_chip;
    } else {
        split_chip = std::make_shared<Chip>(original_chip, chip_layout.n_region_rows, chip_layout.n_region_columns);
        chip = split_chip.get();
    }

    return package_maker->Make(*chip);
}

Chip ChipDataEncoder::Decode(const Package& package) const
{
    return package_maker->Read(package, chip_layout);
}

} // namespace pixel_studies
