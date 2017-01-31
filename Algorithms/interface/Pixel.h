/*! Definition of class Pixel.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <set>
#include <vector>
#include <map>

namespace pixel_studies {

using RawCoordinate = int16_t;
using Adc = uint16_t;

enum class Ordering { ByRow, ByColumn, ByRegionByRow, ByRegionByColumn };

template<typename _Coordinate>
struct Position {
    using Coordinate = _Coordinate;
    Coordinate row, column;

    Position(Coordinate _row, Coordinate _column) : row(_row), column(_column) {}
    Position() : Position(0, 0) {}

    bool operator<(const Position<Coordinate>& other) const
    {
        if(row < other.row) return true;
        if(row > other.row) return false;
        return column < other.column;
    }

    bool operator!=(const Position<Coordinate>& other) const
    {
        return row != other.row || column != other.column;
    }

    bool operator==(const Position<Coordinate>& other) const
    {
        return !(*this != other);
    }
};

using Pixel = Position<RawCoordinate>;
using PixelSet = std::set<Pixel>;
using PixelAdcPair = std::pair<Pixel, Adc>;
using PixelWithAdcVector = std::vector<PixelAdcPair>;
using PixelWithAdcMap = std::map<Pixel, Adc>;

template<typename Coordinate>
std::ostream& operator<<(std::ostream& s, const Position<Coordinate>& p)
{
    s << "(" << p.row << ", " << p.column << ")";
    return s;
}

} // namespace pixel_studies
