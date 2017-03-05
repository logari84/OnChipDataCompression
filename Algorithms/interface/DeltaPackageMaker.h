/*! Implementation of a PackageMaker using delta representation.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "AlphabetStatistics.h"
#include "AlphabetStatisticsCollection.h"
#include "PackageMaker.h"

namespace pixel_studies {

enum class DeltaPackageMakerMode { SeparateDelta, CombinedDelta };

template<typename Decoder>
class DeltaPackageMaker : public PackageMaker {
public:
    using Letter = int;
    using StatisticsSource = AlphabetStatisticsCollection<Letter>;
    using Statistics = StatisticsSource::Statistics;
    using StatisticsPtr = StatisticsSource::StatisticsPtr;
    using Encoder = typename Decoder::Encoder;
    using Coordinate = Pixel::Coordinate;
    using Mode = DeltaPackageMakerMode;

    static constexpr size_t BitsPerNpixels = 10;
    static constexpr Coordinate SpecialLetter = -1;

    DeltaPackageMaker(const StatisticsSource &source, const RegionLayout& _readout_unit_layout, Mode _mode,
                      Ordering _ordering) :
        PackageMaker(0), readout_unit_layout(_readout_unit_layout), mode(_mode), ordering(_ordering),
        adc_stat(source.at(AlphabetType::ActiveAdc))
    {
        if(mode == Mode::SeparateDelta) {
            delta_row_stat = source.at(AlphabetType::DeltaRow);
            delta_column_stat = source.at(AlphabetType::DeltaColumn);
        } else if(mode == Mode::CombinedDelta) {
            delta_rowcolumn_stat = source.at(AlphabetType::DeltaRowColumn);
        } else
            throw exception("Unsupported delta package maker mode.");
    }

    static std::string MakerName(Mode mode)
    {
        static const std::map<DeltaPackageMakerMode, std::string> modeNames = {
            { DeltaPackageMakerMode::SeparateDelta, "separate" },
            { DeltaPackageMakerMode::CombinedDelta, "combined" },
        };
        return modeNames.at(mode) + "_delta_" + Decoder::Name();
    }

    virtual Package Make(const Chip& chip) const override
    {
        using RegionIteratorCollection = std::list<RegionIterator>;
        Package package;
        size_t max_size = 0;
        const auto& multi_layout = chip.GetMultiRegionLayout();
        const auto& layout = multi_layout.region_layout;
        const size_t n_macro_regions = multi_layout.GetNumberOfRegions();
        RegionIteratorCollection region_iterators;

        for(size_t macro_region_id = 0; macro_region_id < n_macro_regions; ++macro_region_id) {
            PixelWithAdcVector pixels = {};
            if(chip.IsRegionActive(macro_region_id)) {
                const PixelMultiRegion pixel_area(chip.GetRegion(macro_region_id), readout_unit_layout);
                pixels = pixel_area.GetOrderedPixels(ordering);
            }
            region_iterators.push_back(RegionIterator(pixels));
            max_size = std::max(max_size, region_iterators.back().size());
        }

        for(size_t n = 0; n < max_size; ++n) {
            for(RegionIterator& region_iter : region_iterators) {
                if(!region_iter.has_current()) continue;
                const Pixel previous_pixel = region_iter.previous().first;
                const Pixel& pixel = region_iter.current().first;
                const Adc& adc = region_iter.current().second;

                EncodePixel(package, layout, pixel, previous_pixel);
                Encoder::EncodeLetter(*adc_stat, adc, package);
                region_iter.move_next();
            }
            if((n+1) % 2 == 0 || (n+1) == max_size)
                package.next_readout_cicle();
        }

        if(n_macro_regions > 1) {
            for(RegionIterator& region_iter : region_iterators)
                package.write(region_iter.size(), BitsPerNpixels);
            package.next_readout_cicle();
        }

        return package;
    }

    virtual Chip Read(const Package &package, const MultiRegionLayout& multi_layout) const override
    {
        Chip chip(multi_layout);
        const size_t n_macro_regions = chip.GetMultiRegionLayout().GetNumberOfRegions();
        const RegionLayout& layout = multi_layout.region_layout;
        std::vector<Pixel> previous_pixel;
        previous_pixel.assign(n_macro_regions, RegionIterator::DefaultPixel().first);
        size_t max_n_pixels = 0;
        std::vector<size_t> n_pixels(n_macro_regions);
        if(n_macro_regions > 1) {
            Package::iterator trailer_iter = package.end();
            trailer_iter -= BitsPerNpixels * n_macro_regions;
            for(size_t k = 0; k < n_macro_regions; ++k) {
                const size_t n = trailer_iter.read(BitsPerNpixels, false);
                max_n_pixels = std::max(max_n_pixels, n);
                n_pixels.at(k) = n;
            }
        } else {
            max_n_pixels = std::numeric_limits<size_t>::max();
            n_pixels.at(0) = max_n_pixels;
        }

        Package::iterator iter = package.begin();
        for(size_t n = 0; n < max_n_pixels && iter != package.end(); ++n) {
            for(size_t k = 0; k < n_macro_regions; ++k) {
                if(n_pixels.at(k) <= n) continue;
                const Pixel region_pixel = DecodePixel(iter, layout, previous_pixel.at(k));
                const Adc adc = Decoder::DecodeLetter(*adc_stat, iter);
                Pixel pixel;
                multi_layout.ConvertFromRegionPixel(k, region_pixel, pixel);
                chip.AddPixel(pixel, adc);
                previous_pixel.at(k) = region_pixel;
            }
        }

        return chip;
    }

private:
    static void EncodeLetter(Package& package, StatisticsPtr stat, Letter letter, size_t abs_value,
                             size_t bits_per_raw_data)
    {
        if(stat->GetAlphabet().count(letter)) {
            Encoder::EncodeLetter(*stat, letter, package);
        } else {
            Encoder::EncodeLetter(*stat, SpecialLetter, package);
            package.write(abs_value, bits_per_raw_data);
        }
    }

    template<typename LetterType, typename AbsValueType>
    static bool DecodeLetter(Package::iterator& iter, StatisticsPtr stat, LetterType& letter, AbsValueType& abs_value,
                             size_t bits_per_raw_data)
    {
        letter = Decoder::DecodeLetter(*stat, iter);
        if(letter == SpecialLetter)
            abs_value = iter.read(bits_per_raw_data, false);
        return letter != SpecialLetter;
    }

    void EncodePixel(Package& package, const RegionLayout& layout, const Pixel& pixel,
                     const Pixel& previous_pixel) const
    {
        const Coordinate delta_row = (pixel.row + layout.n_rows - previous_pixel.row) % layout.n_rows;
        const Coordinate delta_column = (pixel.column + layout.n_columns - previous_pixel.column)
                                        % layout.n_columns;
        if(mode == Mode::SeparateDelta) {
            EncodeLetter(package, delta_row_stat, delta_row, pixel.row, layout.BitsPerRow());
            EncodeLetter(package, delta_column_stat, delta_column, pixel.column, layout.BitsPerColumn());
        } else {
            const size_t delta_rowcolumn = layout.GetPixelId(Pixel(delta_row, delta_column));
            const size_t pixel_id = layout.GetPixelId(pixel);
            EncodeLetter(package, delta_rowcolumn_stat, delta_rowcolumn, pixel_id, layout.BitsPerId());
        }
    }

    Pixel DecodePixel(Package::iterator& iter, const RegionLayout& layout, const Pixel& previous_pixel) const
    {
        Pixel delta, pixel;
        bool has_delta_row = false, has_delta_column = false;
        if(mode == Mode::SeparateDelta) {
            has_delta_row = DecodeLetter(iter, delta_row_stat, delta.row, pixel.row, layout.BitsPerRow());
            has_delta_column = DecodeLetter(iter, delta_column_stat, delta.column, pixel.column, layout.BitsPerColumn());
        } else {
            Letter delta_rowcolumn;
            size_t pixel_id;
            has_delta_row = has_delta_column = DecodeLetter(iter, delta_rowcolumn_stat, delta_rowcolumn,
                                                            pixel_id, layout.BitsPerId());
            if(has_delta_row)
                delta = layout.GetPixel(delta_rowcolumn);
            else
                pixel = layout.GetPixel(pixel_id);
        }
        if(has_delta_row)
            pixel.row = (previous_pixel.row + delta.row) % layout.n_rows;
        if(has_delta_column)
            pixel.column = (previous_pixel.column + delta.column) % layout.n_columns;
        return pixel;
    }


private:
    RegionLayout readout_unit_layout;
    Mode mode;
    Ordering ordering;
    StatisticsPtr adc_stat, delta_row_stat, delta_column_stat, delta_rowcolumn_stat;
};

} // namespace pixel_studies
