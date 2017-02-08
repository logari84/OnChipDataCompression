/*! The base package maker class and its default implementation.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <cmath>
#include "Chip.h"
#include "Package.h"

namespace pixel_studies {

class PackageMaker {
public:
    PackageMaker(size_t _n_bits_per_adc) : n_bits_per_adc(_n_bits_per_adc) {}

    virtual Package Make(const Chip& chip) const = 0;
    virtual Chip Read(const Package& package, const MultiRegionLayout& layout) const = 0;
    virtual ~PackageMaker() {}

    const size_t n_bits_per_adc;
};

class RegionIterator {
public:
    static const PixelAdcPair& DefaultPixel() { static const PixelAdcPair pixel(Pixel(0, 0), 0); return pixel; }
    explicit RegionIterator(const PixelWithAdcVector& _pixels) : pixels(_pixels), current_position(0) {}

    size_t size() const { return pixels.size(); }
    const PixelAdcPair& previous() const
    {
        if(!current_position) return DefaultPixel();
        return pixels.at(current_position - 1);
    }

    bool has_current() const { return current_position < pixels.size(); }

    const PixelAdcPair& current() const
    {
        if(!has_current())
            throw exception("Pixel not available.");
        return pixels.at(current_position);
    }

    void move_next()
    {
        if(!has_current())
            throw exception("Unable to move to the next pixel.");
        ++current_position;
    }

private:
    PixelWithAdcVector pixels;
    size_t current_position;
};

class DefaultPackageMaker : public PackageMaker {
public:
    static std::string MakerName() { return "default"; }

    using PackageMaker::PackageMaker;

    virtual Package Make(const Chip& chip) const override
    {
        using RegionIteratorCollection = std::list<RegionIterator>;
        Package package;
        size_t max_size = 0;
        const auto& multi_layout = chip.GetMultiRegionLayout();
        const size_t n_macro_regions = multi_layout.GetNumberOfRegions();
        const size_t n_bits_per_pixel_id = multi_layout.BitsPerId();
        RegionIteratorCollection region_iterators;

        for(size_t macro_region_id = 0; macro_region_id < n_macro_regions; ++macro_region_id) {
            PixelWithAdcVector pixels = {};
            if(chip.IsRegionActive(macro_region_id)) {
                const auto& region_pixels = chip.GetRegion(macro_region_id).GetPixels();
                for(const auto& pixel_with_adc : region_pixels) {
                    Pixel global_pixel;
                    multi_layout.ConvertFromRegionPixel(macro_region_id, pixel_with_adc.first, global_pixel);
                    pixels.push_back(PixelAdcPair(global_pixel, pixel_with_adc.second));
                }
            }
            region_iterators.push_back(RegionIterator(pixels));
            max_size = std::max(max_size, region_iterators.back().size());
        }

        for(size_t n = 0; n < max_size; ++n) {
            for(RegionIterator& region_iter : region_iterators) {
                if(!region_iter.has_current()) continue;
                const Pixel& pixel = region_iter.current().first;
                const Adc& adc = region_iter.current().second;
                const size_t pixel_id = multi_layout.GetPixelId(pixel);
                package.write(pixel_id, n_bits_per_pixel_id);
                package.write(adc, n_bits_per_adc);
                region_iter.move_next();
            }
            if((n+1) % 2 == 0 || (n+1) == max_size)
                package.next_readout_cicle();
        }

        return package;
    }

    virtual Chip Read(const Package &package, const MultiRegionLayout& layout) const override
    {
        const size_t n_bits_per_pixel_id = layout.BitsPerId();

        Chip chip(layout);

        for(Package::iterator iter = package.begin(); iter != package.end();) {
            const size_t pixel_id = iter.read(n_bits_per_pixel_id);
            const Adc adc = iter.read(n_bits_per_adc);
            const Pixel pixel = layout.GetPixel(pixel_id);
            chip.AddPixel(pixel, adc);
        }

        return chip;
    }
};

} // namespace pixel_studies
