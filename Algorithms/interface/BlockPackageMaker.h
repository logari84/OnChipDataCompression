/*! Implementation of a PackageMaker using per-block representation.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "AlphabetStatistics.h"
#include "AlphabetStatisticsCollection.h"
#include "PackageMaker.h"

namespace pixel_studies {

template<typename Decoder>
class BlockPackageMaker : public PackageMaker {
public:
    using Letter = int;
    using StatisticsSource = AlphabetStatisticsCollection<Letter>;
    using Statistics = StatisticsSource::Statistics;
    using StatisticsPtr = StatisticsSource::StatisticsPtr;
    using Encoder = typename Decoder::Encoder;
    using Coordinate = Pixel::Coordinate;

    BlockPackageMaker(const StatisticsSource* source, const RegionLayout& _readout_unit_layout, size_t _n_bits_per_adc,
                      bool encode_adc) :
        PackageMaker(_n_bits_per_adc), readout_unit_layout(_readout_unit_layout)
    {
        if(encode_adc)
            adc_stat = source->at(AlphabetType::Adc);
    }

    static std::string MakerName(bool encode_adc)
    {
        return encode_adc ? "block_encoded" : "block_raw";
    }

    static size_t GetFullRegionId(size_t macro_region_id, size_t region_id, size_t n_macro_regions)
    {
        return region_id * n_macro_regions + macro_region_id;
    }

    static void SplitFullRegionId(size_t full_region_id, size_t n_macro_regions,
                                  size_t& macro_region_id, size_t& region_id)
    {
        macro_region_id = full_region_id % n_macro_regions;
        region_id = (full_region_id - macro_region_id) / n_macro_regions;
    }


    virtual Package Make(const Chip& chip) const override
    {
        using RegionVector = std::list<std::pair<size_t, PixelRegion>>;
        using MacroRegionDescriptor = std::pair<size_t, RegionVector>;
        using MacroRegionCollection = std::list<MacroRegionDescriptor>;

        const auto& multi_layout = chip.GetMultiRegionLayout();
        const size_t n_macro_regions = multi_layout.GetNumberOfRegions();
        size_t n_regions = 0;
        MacroRegionCollection region_pixels;

        for(size_t macro_region_id = 0; macro_region_id < n_macro_regions; ++macro_region_id) {
            if(!chip.IsRegionActive(macro_region_id)) continue;
            const PixelMultiRegion pixel_area(chip.GetRegion(macro_region_id), readout_unit_layout);
            n_regions = pixel_area.GetMultiRegionLayout().GetNumberOfRegions();
            MacroRegionDescriptor active_regions;
            active_regions.first = macro_region_id;
            for(size_t region_id = 0; region_id < n_regions; ++region_id) {
                if(!pixel_area.IsRegionActive(region_id)) continue;
                const PixelRegion& region = pixel_area.GetRegion(region_id);
                active_regions.second.push_back(std::make_pair(region_id, region));
            }
            if(active_regions.second.size())
                region_pixels.push_back(active_regions);
        }

        const size_t n_bits_per_address = RegionLayout::BitsPerValue(n_regions * n_macro_regions);

        Package package;
        while(region_pixels.size()) {
            for(auto macro_region_iter = region_pixels.begin(); macro_region_iter != region_pixels.end();) {
                const size_t macro_region_id = macro_region_iter->first;
                auto region_iter = macro_region_iter->second.begin();
                const size_t region_id = region_iter->first;
                const PixelRegion& region = region_iter->second;

                const size_t full_region_id = GetFullRegionId(macro_region_id, region_id, n_macro_regions);
                package.write(full_region_id, n_bits_per_address);

                for(size_t row = 0; row < readout_unit_layout.n_rows; ++row) {
                    for(size_t column = 0; column < readout_unit_layout.n_columns; ++column) {
                        const Adc adc = region.GetAdc(row, column);
                        if(adc_stat)
                            Encoder::EncodeLetter(*adc_stat, adc, package);
                        else
                            package.write(adc, n_bits_per_adc);
                    }
                }

                macro_region_iter->second.erase(region_iter);
                auto prev_iter = macro_region_iter;
                ++macro_region_iter;
                if(!prev_iter->second.size())
                    region_pixels.erase(prev_iter);
            }
            package.next_readout_cicle();
        }

        return package;
    }

    virtual Chip Read(const Package &package, const MultiRegionLayout& multi_layout) const override
    {
        Chip chip(multi_layout);
        const size_t n_macro_regions = multi_layout.GetNumberOfRegions();
        const MultiRegionLayout layout(multi_layout.region_layout.n_rows, multi_layout.region_layout.n_columns,
                                       readout_unit_layout);
        const size_t n_regions = layout.GetNumberOfRegions();
        const size_t n_bits_per_address = RegionLayout::BitsPerValue(n_regions * n_macro_regions);

        for(Package::iterator iter = package.begin(); iter != package.end();) {
            const size_t full_region_id = iter.read(n_bits_per_address);
            size_t macro_region_id, region_id;
            SplitFullRegionId(full_region_id, n_macro_regions, macro_region_id, region_id);

            for(size_t row = 0; row < readout_unit_layout.n_rows; ++row) {
                for(size_t column = 0; column < readout_unit_layout.n_columns; ++column) {
                    const Adc adc = adc_stat ? Decoder::DecodeLetter(*adc_stat, iter) : iter.read(n_bits_per_adc);
                    if(adc) {
                        const Pixel readout_pixel(row, column);
                        Pixel macro_region_pixel;
                        layout.ConvertFromRegionPixel(region_id, readout_pixel, macro_region_pixel);
                        Pixel chip_pixel;
                        multi_layout.ConvertFromRegionPixel(macro_region_id, macro_region_pixel, chip_pixel);
                        chip.AddPixel(chip_pixel, adc);
                    }
                }
            }
        }
        return chip;
    }

private:
    StatisticsPtr adc_stat;
    RegionLayout readout_unit_layout;
};

} // namespace pixel_studies
