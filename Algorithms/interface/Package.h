/*! The class that represents a data package.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <vector>
#include "exception.h"

namespace pixel_studies {

struct Package {
    using Integer = uint64_t;
    using DataContainer = std::vector<uint8_t>;

    static constexpr size_t BitsPerByte = std::numeric_limits<uint8_t>::digits;
    static constexpr size_t BitsPerItem = std::numeric_limits<DataContainer::value_type>::digits;
    static constexpr size_t BitsPerInteger = std::numeric_limits<Integer>::digits;

    class iterator {
    public:
        explicit iterator(const Package& _package, size_t _position = 0) : package(&_package), pos(_position) {}
        Integer read(size_t number_of_bits_requested, bool use_zeros_for_missing_data = false);
        Integer read_ex(size_t number_of_bits_requested, bool use_zeros_for_missing_data = false);
        size_t position() const { return pos; }
        size_t item_position() const { return position() / BitsPerItem; }
        size_t shift() const { return position() % BitsPerItem; }

        void check() const
        {
            if(position() > package->end().position())
              throw exception("Position is is beyong the end of the package.");
        }

        iterator& operator+=(size_t delta) { pos += delta; return *this; }
        iterator& operator-=(size_t delta)
        {
            if(delta > position())
                throw exception("Delta is too big.");
            pos -= delta;
            return *this;
        }

        size_t operator-(const iterator& other) const
        {
            if(other.package != package)
                throw exception("Difference between iterators from two different packages.");
            if(position() < other.position())
                throw exception("Negative difference between iterators is not supported.");
            return position() - other.position();
        }

        bool operator==(const iterator& other) const { return package == other.package && pos == other.pos; }
        bool operator!=(const iterator& other) const { return !(*this == other); }


    private:
        const Package* package;
        size_t pos;
    };

    using PositionCollection = std::vector<size_t>;

    Package() : begin_iter(*this), end_iter(*this) {}
    Package(const Package& other)
        : data(other.data), begin_iter(*this, other.begin_iter.position()),
          end_iter(*this, other.end_iter.position()) {}

    DataContainer& container() { return data; }
    const DataContainer& container() const { return data; }
    const PositionCollection& readout_positions() const { return readout_position_collection; }

    /// Returns full package size in bits.
    size_t size() const { return end_iter.position(); }
    const iterator& begin() const { return begin_iter; }
    const iterator& end() const { return end_iter; }

    void write(Integer value, size_t number_of_bits)
    {
        if(number_of_bits > BitsPerInteger)
            throw exception("Number of bits is too big.");
        else if(number_of_bits < BitsPerInteger) {
            const Integer max_input_value = (Integer(1) << number_of_bits) - 1;
            if(value > max_input_value)
                throw exception("Input value = %1% is too big. Max value for n_bits = %2% is %3%.")
                        % value % number_of_bits % max_input_value;
        }
        for(size_t n = 0; n < number_of_bits; ++n) {
            const size_t shift = number_of_bits - n - 1;
            const Integer bit = (value >> shift) & Integer(1);
            write_ex(bit, 1);
        }
    }

    void write_ex(Integer value, size_t number_of_bits)
    {
        if(number_of_bits > BitsPerInteger)
            throw exception("Number of bits is too big.");
        else if(number_of_bits < BitsPerInteger) {
            const Integer max_input_value = (Integer(1) << number_of_bits) - 1;
            if(value > max_input_value)
                throw exception("Input value = %1% is too big. Max value for n_bits = %2% is %3%.")
                        % value % number_of_bits % max_input_value;
        }

        size_t n_written = 0;
        while(n_written < number_of_bits) {
            const size_t current_shift = end_iter.shift();

            if(!current_shift)
                data.push_back(0);
            const size_t delta = number_of_bits - n_written;
            const size_t n_to_write = std::min(BitsPerItem - current_shift, delta);
            const Integer mask = Mask(n_to_write);
            const Integer masked_value = (value >> n_written) & mask;
            const Integer max_to_write = (Integer(1) << n_to_write) - 1;
            if(masked_value > max_to_write)
                throw exception("shifted value is too big");
            const Integer value_to_write = masked_value << current_shift;
            data.back() |= value_to_write;
            n_written += n_to_write;
            end_iter += n_to_write;
        }
    }

    void write(const Package& other)
    {
        for(iterator iter = other.begin(); iter != other.end();) {
            const size_t n_to_read = std::min(size_t(BitsPerInteger), other.end() - iter);
            const Integer value = iter.read(n_to_read, false);
            write(value, n_to_read);
        }
    }

    void finalize_byte()
    {
        const size_t n_written = end_iter.position() % BitsPerByte;
        const size_t n_to_write = n_written ? BitsPerByte - n_written : 0;
        write(0, n_to_write);
    }

    void next_readout_cicle() { readout_position_collection.push_back(end().position()); }

    bool operator==(const Package& other) const
    {
        if(end_iter.position() != other.end_iter.position()) return false;
        for(size_t n = 0; n < data.size(); ++n) {
            if(data.at(n) != other.data.at(n)) return false;
        }
        return true;
    }
    bool operator!=(const Package& other) const { return !(*this == other); }

    static Integer Mask(const size_t n_bits)
    {
        return n_bits == BitsPerInteger
                ? std::numeric_limits<Integer>::max()
                : (Integer(1) << n_bits) - 1;
    }

private:
    DataContainer data;
    iterator begin_iter, end_iter;
    PositionCollection readout_position_collection;
};

Package::Integer Package::iterator::read(size_t number_of_bits_requested, bool use_zeros_for_missing_data)
{
    if(number_of_bits_requested > std::numeric_limits<Integer>::digits)
        throw exception("Number of bits to read is too big.");
    const size_t bits_left = package->end().position() - position();
    if(number_of_bits_requested > bits_left && !use_zeros_for_missing_data)
        throw exception("No enough data in the package to perform read operation."
            " Number of bits requested = %1%, number of bits left = %2%.") % number_of_bits_requested % bits_left;

    const size_t number_of_bits = std::min(number_of_bits_requested, bits_left);
    Integer result = 0;
    for(size_t n_read = 0; n_read < number_of_bits; ++n_read) {
        result = (result << 1) + read_ex(1, false);
    }

    result <<= number_of_bits_requested - number_of_bits;
    return result;
}

Package::Integer Package::iterator::read_ex(size_t number_of_bits_requested, bool use_zeros_for_missing_data)
{
    if(number_of_bits_requested > std::numeric_limits<Integer>::digits)
        throw exception("Number of bits to read is too big.");
    const size_t bits_left = package->end().position() - position();
    if(number_of_bits_requested > bits_left && !use_zeros_for_missing_data)
        throw exception("No enough data in the package to perform read operation."
            " Number of bits requested = %1%, number of bits left = %2%.") % number_of_bits_requested % bits_left;

    const size_t number_of_bits = std::min(number_of_bits_requested, bits_left);
    Integer result = 0;
    size_t n_read = 0;
    while(n_read < number_of_bits) {
        const size_t n_to_read = std::min(BitsPerItem - shift(), number_of_bits - n_read);
        Integer data = package->container().at(item_position());
        data >>= shift();
        const Integer mask = Mask(n_to_read);
        data &= mask;
        data <<= n_read;
        result |= data;
        n_read += n_to_read;
        pos += n_to_read;
    }

    result <<= number_of_bits_requested - number_of_bits;
    return result;
}

} // namespace pixel_studies
