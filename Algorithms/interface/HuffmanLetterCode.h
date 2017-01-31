/*! The class that represents the Huffman code for a single letter.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <vector>
#include <map>
#include <sstream>

#include "exception.h"

namespace pixel_studies {

class HuffmanCode {
public:
    using CodeContainer = uint64_t;
    static constexpr size_t MaxNumberOfBits = std::numeric_limits<CodeContainer>::digits;

public:
    HuffmanCode() : code(0), n_bits(0) {}

    explicit HuffmanCode(bool _code)
        : code(0), n_bits(0)
    {
        Append(_code);
    }

    HuffmanCode(const CodeContainer& _code, size_t _n_bits) : code(_code), n_bits(_n_bits) {}

    HuffmanCode(const HuffmanCode& prefix, bool _code)
        : code(prefix.code), n_bits(prefix.n_bits)
    {
        Append(_code);
    }

    size_t NumberOfBits() const { return n_bits; }
    const CodeContainer& Code() const { return code; }

    bool operator==(const HuffmanCode& other) const
    {
        return code == other.code && n_bits == other.n_bits;
    }
    bool operator!=(const HuffmanCode& other) const { return !(*this == other); }

    bool operator<(const HuffmanCode& other) const
    {
        if(n_bits != other.n_bits) return n_bits < other.n_bits;
        return code < other.code;
    }

private:
    void Append(bool _code)
    {
        if(n_bits + 1 > MaxNumberOfBits)
            throw exception("Huffman code is too long.");
        code = (CodeContainer(_code) << n_bits) | code;
        ++n_bits;
    }

private:
    CodeContainer code;
    size_t n_bits;
};

inline std::ostream& operator<<(std::ostream& os, const HuffmanCode& huffman_code)
{
    std::ostringstream ss;
    for(size_t n = 0; n < huffman_code.NumberOfBits(); ++n) {
        const HuffmanCode::CodeContainer bit = (huffman_code.Code() >> n) & HuffmanCode::CodeContainer(1);
        ss << bit;
    }
    os << ss.str();
    return os;
}

inline std::istream& operator>>(std::istream& is, HuffmanCode& code)
{
    static const std::map<char, bool> char_codes = { { '0', false }, { '1', true } };
    std::string s_bits;
    is >> s_bits;
    const size_t n_bits = s_bits.size();
    code = HuffmanCode();
    for(size_t n = 0; n < n_bits; ++n) {
        const char c = s_bits.at(n);
        if(!char_codes.count(c))
            throw exception("Invalid Huffman code '%1%'.") % s_bits;
        code = HuffmanCode(code, char_codes.at(c));
    }
    return is;
}

} // namespace pixel_studies
