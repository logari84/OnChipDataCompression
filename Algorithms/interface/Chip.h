/*! The classes that define chip layout and content.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "exception.h"
#include "Pixel.h"

namespace pixel_studies {

struct RegionLayout {
    size_t n_rows, n_columns;

    RegionLayout(size_t _n_rows, size_t _n_columns);
    virtual ~RegionLayout() {}

    void CheckPixel(const Pixel& pixel) const;
    bool IsPixelInside(const Pixel& pixel) const;
    size_t GetPixelId(const Pixel& pixel) const;
    Pixel GetPixel(size_t pixel_id) const;

    size_t GetNumberOfPixels() const { return n_rows * n_columns; }
    static size_t BitsPerValue(size_t max_value) { return std::ceil(std::log2(double(max_value))); }
    size_t BitsPerRow() const { return BitsPerValue(n_rows); }
    size_t BitsPerColumn() const { return BitsPerValue(n_columns); }
    size_t BitsPerId() const { return BitsPerValue(GetNumberOfPixels()); }

    bool operator==(const RegionLayout& other) const { return n_rows == other.n_rows && n_columns == other.n_columns; }
    bool operator!=(const RegionLayout& other) const { return !(*this == other); }
};

struct MultiRegionLayout : RegionLayout {
    RegionLayout region_layout;
    size_t n_region_rows, n_region_columns;
    size_t n_last_region_rows, n_last_region_columns;

    MultiRegionLayout(size_t _n_rows, size_t _n_columns, const RegionLayout& _region_layout);
    MultiRegionLayout(size_t _n_rows, size_t _n_columns, size_t _n_region_rows, size_t _n_region_columns);
    MultiRegionLayout(size_t _n_rows, size_t _n_columns);
    MultiRegionLayout(const RegionLayout& other, size_t _n_region_rows, size_t _n_region_columns);

    size_t GetNumberOfRegions() const { return n_region_rows * n_region_columns; }

    void ConvertToRegionPixel(const Pixel& pixel, size_t& region_id, Pixel& region_pixel) const;
    void ConvertFromRegionPixel(size_t region_id, const Pixel& region_pixel, Pixel& pixel) const;
    size_t GetRegionId(size_t region_row_index, size_t region_column_index) const;
    RegionLayout ActualRegionLayout(size_t region_id) const;
    bool IsRegionComplete(size_t region_id) const;

    bool operator==(const MultiRegionLayout& other) const;
    bool operator!=(const MultiRegionLayout& other) const { return !(*this == other); }
};

class PixelRegion {
public:
    explicit PixelRegion(const RegionLayout& _region_layout) : region_layout(_region_layout) {}
    virtual ~PixelRegion() {}

    const RegionLayout& GetRegionLayout() const { return region_layout; }
    size_t GetNumberOfRows() const { return region_layout.n_rows; }
    size_t GetNumberOfColumns() const { return region_layout.n_columns; }
    const PixelWithAdcMap& GetPixels() const { return pixels; }
    Adc GetAdc(const Pixel& pixel) const;
    Adc GetAdc(size_t row, size_t column) const { return GetAdc(Pixel(row, column)); }
    bool HasActivePixels() const { return pixels.size() != 0; }

    virtual void AddPixel(const Pixel& pixel, Adc adc);
    virtual PixelWithAdcVector GetOrderedPixels(Ordering ordering) const;
    bool HasSamePixels(const PixelRegion& other, std::ostream* os = nullptr) const;

private:
    RegionLayout region_layout;
    PixelWithAdcMap pixels;
};

class PixelMultiRegion : public PixelRegion {
public:
    using RegionPtr = std::shared_ptr<PixelRegion>;
    using RegionVector = std::vector<RegionPtr>;

    explicit PixelMultiRegion(const MultiRegionLayout& _multi_layout);
    PixelMultiRegion(const PixelRegion& original, size_t n_region_rows, size_t n_region_columns);
    PixelMultiRegion(const PixelRegion& original, const RegionLayout& _region_layout);

    virtual void AddPixel(const Pixel& pixel, Adc adc) override;
    virtual PixelWithAdcVector GetOrderedPixels(Ordering ordering) const override;
    const PixelRegion& GetRegion(size_t region_id) const;
    const PixelRegion& GetRegion(size_t region_row_index, size_t region_column_index) const;
    bool IsRegionActive(size_t region_id) const;
    bool IsRegionActive(size_t region_row_index, size_t region_column_index) const;
    const MultiRegionLayout& GetMultiRegionLayout() const { return multi_region_layout; }

    bool operator==(const PixelMultiRegion& other) const { return HasSamePixels(other); }
    bool operator!=(const PixelMultiRegion& other) const { return !(*this == other); }

private:
    void CreateRegions();
    void AddPixelToRegion(const Pixel& pixel, Adc adc);

private:
    MultiRegionLayout multi_region_layout;
    RegionVector regions;
};

using Chip = PixelMultiRegion;
using ChipPtr = std::shared_ptr<Chip>;
using ChipPtrVector = std::vector<ChipPtr>;

} // namespace pixel_studies
