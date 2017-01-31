/*! Implementation of the Huffman decoder.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "Package.h"
#include "HuffmanLetterCode.h"

namespace pixel_studies {

class HuffmanEncoder;

class HuffmanDecoder {
public:
    using Encoder = HuffmanEncoder;

    static const std::string& Name() { static const std::string name = "huffman"; return name; }

    template<typename OutputCollection, typename Statistics>
    static void Decode(const Statistics& statistics, OutputCollection& outputCollection,
                       Package::iterator& inputIterator, size_t n_expected)
    {
        typedef typename Statistics::Letter Letter;

        HuffmanCode code;
        for(size_t n_decoded = 0; n_decoded < n_expected;) {

            const bool bit = inputIterator.read(1);
            code = HuffmanCode(code, bit);
            Letter letter;
            if(statistics.GetLetterFromHuffmanCode(code, letter)) {
                outputCollection.push_back(letter);
                code = HuffmanCode();
                ++n_decoded;
            }
        }
    }

    template<typename Statistics>
    static typename Statistics::Letter DecodeLetter(const Statistics& statistics, Package::iterator& inputIterator)
    {
        typedef typename Statistics::Letter Letter;

        HuffmanCode code;
        Letter letter;
        do {
            const bool bit = inputIterator.read(1);
            code = HuffmanCode(code, bit);
        } while(!statistics.GetLetterFromHuffmanCode(code, letter));
        return letter;
    }


private:
    ~HuffmanDecoder() {}
};

} // namespace pixel_studies
