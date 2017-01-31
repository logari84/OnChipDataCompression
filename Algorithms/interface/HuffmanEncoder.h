/*! Implementation of the Huffman encoder.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "Package.h"
#include "HuffmanLetterCode.h"

namespace pixel_studies {

class HuffmanEncoder {
public:
    template<typename InputCollection, typename Statistics>
    static void Encode(const Statistics& statistics, const InputCollection& inputCollection, Package& package)
    {
        for(const auto& letter : inputCollection) {
            const auto& code = statistics.GetHuffmanCode(letter);

            for(const bool& bit : code.Code())
                package.write(bit, 1);
        }

        package.finalize_byte();
    }

    template<typename Statistics>
    static void EncodeLetter(const Statistics& statistics, const typename Statistics::Letter& letter, Package& package)
    {
        const HuffmanCode& code = statistics.GetHuffmanCode(letter);
        for(size_t n = 0; n < code.NumberOfBits(); ++n) {
            const HuffmanCode::CodeContainer bit = (code.Code() >> n) & HuffmanCode::CodeContainer(1);
            package.write(bit, 1);
        }

//        package.write(code.Code(), code.NumberOfBits());
    }

private:
    ~HuffmanEncoder() {}
};

} // namespace pixel_studies
