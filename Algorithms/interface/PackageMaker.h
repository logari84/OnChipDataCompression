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

class DefaultPackageMaker : public PackageMaker {
public:
    static std::string MakerName() { return "default"; }

    using PackageMaker::PackageMaker;

    virtual Package Make(const Chip& chip) const override
    {
        Package package;

        const auto& layout = chip.GetMultiRegionLayout();
        const size_t n_bits_per_pixel_id = layout.BitsPerId();
        const size_t n_regions = layout.GetNumberOfRegions();

        size_t n = 0;
        for(const auto& pixel_entry : chip.GetPixels()) {
            const size_t pixel_id = layout.GetPixelId(pixel_entry.first);
            package.write(pixel_id, n_bits_per_pixel_id);
            package.write(pixel_entry.second, n_bits_per_adc);
            ++n;
            if(n % n_regions == 0 || n == chip.GetPixels().size())
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
