/*! The classes that define chip layout and content.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#include <functional>
#include "../interface/Chip.h"

namespace pixel_studies {

namespace pixel_orderers {
    using PixelOrderer = bool (*)(const PixelAdcPair&, const PixelAdcPair&);
    bool ByRow(const PixelAdcPair& first, const PixelAdcPair& second)
    {
        return first.first.row == second.first.row
                ? first.first.column < second.first.column
                : first.first.row < second.first.row;
    }

    bool ByColumn(const PixelAdcPair& first, const PixelAdcPair& second)
    {
        return first.first.column == second.first.column
                ? first.first.row < second.first.row
                : first.first.column < second.first.column;
    }
} // namespace pixel_orderers

RegionLayout::RegionLayout(size_t _n_rows, size_t _n_columns) :
    n_rows(_n_rows), n_columns(_n_columns)
{
    if(!n_rows || !n_columns)
        throw exception("Invalid region dimensions %1%x%2%.") % n_rows % n_columns;
}

void RegionLayout::CheckPixel(const Pixel& pixel) const
{
    static const std::string error_message = "Pixel %1% = %2% is outside of the region interval [0, %3%].";
    if(pixel.row < 0 || size_t(pixel.row) >= n_rows)
        throw exception(error_message) % "row" % pixel.row % (n_rows - 1);
    if(pixel.column < 0 || size_t(pixel.column) >= n_columns)
        throw exception(error_message) % "column" % pixel.column % (n_columns - 1);
}

bool RegionLayout::IsPixelInside(const Pixel& pixel) const
{
    return pixel.row >= 0 && size_t(pixel.row) < n_rows && pixel.column >= 0 && size_t(pixel.column) < n_columns;
}

size_t RegionLayout::GetPixelId(const Pixel& pixel) const
{
    CheckPixel(pixel);
    return pixel.row * n_columns + pixel.column;
}

Pixel RegionLayout::GetPixel(size_t pixel_id) const
{
    const size_t column = pixel_id % n_columns;
    const size_t row = (pixel_id - column) / n_columns;
    const Pixel pixel(row, column);
    CheckPixel(pixel);
    return pixel;
}

MultiRegionLayout::MultiRegionLayout(size_t _n_rows, size_t _n_columns, const RegionLayout& _region_layout) :
    RegionLayout(_n_rows, _n_columns), region_layout(_region_layout)
{
    n_region_rows = std::ceil(static_cast<double>(n_rows) / region_layout.n_rows);
    n_region_columns = std::ceil(static_cast<double>(n_columns) / region_layout.n_columns);
    if(!n_region_rows || !n_region_columns)
        throw exception("Invalid multi-region layout.");
    n_last_region_rows = n_rows - (n_region_rows - 1) * region_layout.n_rows;
    n_last_region_columns = n_columns - (n_region_columns - 1) * region_layout.n_columns;
}

MultiRegionLayout::MultiRegionLayout(size_t _n_rows, size_t _n_columns,
                                     size_t _n_region_rows, size_t _n_region_columns) :
    RegionLayout(_n_rows, _n_columns), region_layout(1, 1), n_region_rows(_n_region_rows),
    n_region_columns(_n_region_columns)
{
    region_layout.n_rows = std::ceil(static_cast<double>(n_rows) / n_region_rows);
    region_layout.n_columns = std::ceil(static_cast<double>(n_columns) / n_region_columns);
    n_region_rows = std::ceil(static_cast<double>(n_rows) / region_layout.n_rows);
    n_region_columns = std::ceil(static_cast<double>(n_columns) / region_layout.n_columns);
    if(!n_region_rows || !n_region_columns)
        throw exception("Invalid multi-region layout.");
    n_last_region_rows = n_rows - (n_region_rows - 1) * region_layout.n_rows;
    n_last_region_columns = n_columns - (n_region_columns - 1) * region_layout.n_columns;
}

MultiRegionLayout::MultiRegionLayout(size_t _n_rows, size_t _n_columns) :
    MultiRegionLayout(_n_rows, _n_columns, RegionLayout(_n_rows, _n_columns)) {}

MultiRegionLayout::MultiRegionLayout(const RegionLayout& other, size_t _n_region_rows, size_t _n_region_columns) :
    MultiRegionLayout(other.n_rows, other.n_columns, _n_region_rows, _n_region_columns) {}

void MultiRegionLayout::ConvertToRegionPixel(const Pixel& pixel, size_t& region_id, Pixel& region_pixel) const
{
    const size_t region_row_index = pixel.row / region_layout.n_rows;
    const size_t region_column_index = pixel.column / region_layout.n_columns;
    region_id = region_row_index * n_region_columns + region_column_index;
    region_pixel.row = pixel.row % region_layout.n_rows;
    region_pixel.column = pixel.column % region_layout.n_columns;
}

void MultiRegionLayout::ConvertFromRegionPixel(size_t region_id, const Pixel& region_pixel, Pixel& pixel) const
{
    const size_t region_column_index = region_id % n_region_columns;
    const size_t region_row_index = (region_id - region_column_index) / n_region_columns;
    pixel.row = region_row_index * region_layout.n_rows + region_pixel.row;
    pixel.column = region_column_index * region_layout.n_columns + region_pixel.column;
}

size_t MultiRegionLayout::GetRegionId(size_t region_row_index, size_t region_column_index) const
{
    return region_row_index * n_region_columns + region_column_index;
}

RegionLayout MultiRegionLayout::ActualRegionLayout(size_t region_id) const
{
    const size_t region_column_index = region_id % n_region_columns;
    const size_t region_row_index = (region_id - region_column_index) / n_region_columns;
    const size_t region_n_columns = region_column_index + 1 == n_region_columns
            ? n_last_region_columns : region_layout.n_columns;
    const size_t region_n_rows = region_row_index + 1 == n_region_rows
            ? n_last_region_rows : region_layout.n_rows;
    return RegionLayout(region_n_rows, region_n_columns);
}

bool MultiRegionLayout::IsRegionComplete(size_t region_id) const
{
    const RegionLayout actual_region_layout = ActualRegionLayout(region_id);
    return actual_region_layout == region_layout;
}

bool MultiRegionLayout::operator==(const MultiRegionLayout& other) const
{
    return region_layout == other.region_layout && n_region_rows == other.n_region_rows
            && n_region_columns == other.n_region_columns;
}

void PixelRegion::AddPixel(const Pixel& pixel, Adc adc)
{
    region_layout.CheckPixel(pixel);
    if(pixels.count(pixel))
        throw exception("Pixel is alrady present.");
    pixels[pixel] = adc;
}

PixelWithAdcVector PixelRegion::GetOrderedPixels(Ordering ordering) const
{
    static const std::map<Ordering, pixel_orderers::PixelOrderer> orderers {
        { Ordering::ByRow, &pixel_orderers::ByRow },
        { Ordering::ByColumn, &pixel_orderers::ByColumn }
    };
    if(!orderers.count(ordering))
        throw exception("Unsupported ordering");
    PixelWithAdcVector result(pixels.begin(), pixels.end());
    std::sort(result.begin(), result.end(), orderers.at(ordering));
    return result;
}

Adc PixelRegion::GetAdc(const Pixel& pixel) const
{
    const auto iter = pixels.find(pixel);
    return iter != pixels.end() ? iter->second : 0;
}

bool PixelRegion::HasSamePixels(const PixelRegion& other, std::ostream* os) const
{
    if(os)
        *os << "this vs. other" << "\nsize: " << pixels.size() << " - " << other.pixels.size() << "\n";
    if(pixels.size() != other.pixels.size()) return false;
    auto other_iter = other.pixels.begin();
    for(auto this_iter = pixels.begin(); this_iter != pixels.end(); ++this_iter, ++other_iter) {
        if(os) {
            *os << "this pixel: " << this_iter->first << " adc = " << (unsigned) this_iter->second
                << "\nother pixel: " << other_iter->first << " adc = " << (unsigned) other_iter->second << ".\n";
        }
        if(*this_iter != *other_iter)
            return false;
    }
    return true;
}

PixelMultiRegion::PixelMultiRegion(const MultiRegionLayout& _multi_layout) :
    PixelRegion(_multi_layout), multi_region_layout(_multi_layout)
{
    CreateRegions();
}

PixelMultiRegion::PixelMultiRegion(const PixelRegion& original, size_t n_region_rows, size_t n_region_columns) :
    PixelRegion(original), multi_region_layout(original.GetRegionLayout(), n_region_rows, n_region_columns)
{
    CreateRegions();
}

PixelMultiRegion::PixelMultiRegion(const PixelRegion& original, const RegionLayout& _region_layout) :
    PixelRegion(original),
    multi_region_layout(original.GetRegionLayout().n_rows, original.GetRegionLayout().n_columns, _region_layout)
{
    CreateRegions();
}

void PixelMultiRegion::CreateRegions()
{
    if(multi_region_layout.GetNumberOfRegions() > 1) {
        regions.resize(multi_region_layout.GetNumberOfRegions());

        for(const auto& pixel_with_adc : GetPixels())
            AddPixelToRegion(pixel_with_adc.first, pixel_with_adc.second);
    }
}

void PixelMultiRegion::AddPixel(const Pixel& pixel, Adc adc)
{
    PixelRegion::AddPixel(pixel, adc);
    AddPixelToRegion(pixel, adc);
}

void PixelMultiRegion::AddPixelToRegion(const Pixel& pixel, Adc adc)
{
    if(multi_region_layout.GetNumberOfRegions() <= 1) return;
    Pixel region_pixel;
    size_t region_id;
    multi_region_layout.ConvertToRegionPixel(pixel, region_id, region_pixel);
    auto& region = regions.at(region_id);
    if(!region)
        region = std::make_shared<PixelRegion>(multi_region_layout.region_layout);
    region->AddPixel(region_pixel, adc);
}

const PixelRegion& PixelMultiRegion::GetRegion(size_t region_id) const
{
    if(!IsRegionActive(region_id))
        throw exception("Region %1% is not active.") % region_id;
    if(multi_region_layout.GetNumberOfRegions() == 1)
        return *this;
    return *regions.at(region_id);
}

const PixelRegion& PixelMultiRegion::GetRegion(size_t region_row_index, size_t region_column_index) const
{
    const size_t region_id = multi_region_layout.GetRegionId(region_row_index, region_column_index);
    return GetRegion(region_id);
}

bool PixelMultiRegion::IsRegionActive(size_t region_id) const
{
    if(region_id >= multi_region_layout.GetNumberOfRegions())
        throw exception("Invalid region id = %1%.") % region_id;
    if(multi_region_layout.GetNumberOfRegions() == 1)
        return GetPixels().size();
    return regions.at(region_id) != nullptr;
}

bool PixelMultiRegion::IsRegionActive(size_t region_row_index, size_t region_column_index) const
{
    const size_t region_id = multi_region_layout.GetRegionId(region_row_index, region_column_index);
    return IsRegionActive(region_id);
}

PixelWithAdcVector PixelMultiRegion::GetOrderedPixels(Ordering ordering) const
{
    if(ordering != Ordering::ByRegionByRow && ordering != Ordering::ByRegionByColumn)
        return PixelRegion::GetOrderedPixels(ordering);

    using getRegionIdFn = std::function<size_t(size_t, size_t)>;
    const getRegionIdFn getRegionIdByRow = [&](size_t n, size_t k) -> size_t {
        return multi_region_layout.GetRegionId(n, k);
    };
    const getRegionIdFn getRegionIdByColumn = [&](size_t n, size_t k) -> size_t {
        return multi_region_layout.GetRegionId(k, n);
    };
    const auto getRegionId = ordering == Ordering::ByRegionByRow ? getRegionIdByRow : getRegionIdByColumn;

    using getRegionNumberFn = std::function<std::pair<size_t, size_t>()>;
    const getRegionNumberFn getRegionNumbersByRow = [&]() {
        return std::make_pair(multi_region_layout.n_region_rows, multi_region_layout.n_region_columns);
    };
    const getRegionNumberFn getRegioNumbersByColumn = [&]() {
        return std::make_pair(multi_region_layout.n_region_columns, multi_region_layout.n_region_rows);
    };

    const auto getRegionNumbers = ordering == Ordering::ByRegionByRow ? getRegionNumbersByRow : getRegioNumbersByColumn;

    PixelWithAdcVector result;
    const auto n_regions = getRegionNumbers();
    for(size_t n = 0; n < n_regions.first; ++n) {
        for(size_t k = 0; k < n_regions.second; ++k) {
            const size_t region_id = getRegionId(n, k);
            if(!IsRegionActive(region_id)) continue;
            const auto& pixels = GetRegion(region_id).GetPixels();
            for(const auto& pixel_with_adc : pixels) {
                Pixel global_pixel;
                multi_region_layout.ConvertFromRegionPixel(region_id, pixel_with_adc.first, global_pixel);
                result.push_back(PixelAdcPair(global_pixel, pixel_with_adc.second));
            }
        }
    }

    return result;
}

} // namespace pixel_studies
