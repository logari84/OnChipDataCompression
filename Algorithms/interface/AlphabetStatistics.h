/*! The class that collects statistics for a given alphabet.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/bimap.hpp>

#include "HuffmanLetterCode.h"

namespace pixel_studies {
template<typename _Letter>
class AlphabetStatistics {
public:
    using Letter = _Letter;
    using Alphabetum = std::set<Letter>;
    using Integer = uint64_t;
    using Real = double;
    using LetterProbabilityMap = std::map<Letter, Real>;
    using HuffmanTable = boost::bimap<Letter, HuffmanCode>;

    AlphabetStatistics(const std::string& _name, const Alphabetum& _alphabet, Integer _original_counts,
                       const LetterProbabilityMap& _original_probabilities, Real _entropy,
                       const HuffmanTable& _huffman_table)
        : name(_name), alphabet(_alphabet), original_counts(_original_counts),
          original_probabilities(_original_probabilities), entropy(_entropy), huffman_table(_huffman_table)
    {
        if(entropy < 0)
            throw std::runtime_error("Entropy should be a positive number or zero.");
        if(original_counts <= 0)
            throw std::runtime_error("Original counts should be a positive number.");
        if(!alphabet.size())
            throw std::runtime_error("Alphabet is empty.");
        Real total_original_probability = 0;
        for(auto letter_iter = alphabet.begin(); letter_iter != alphabet.end(); ++letter_iter) {
            const Letter& letter = *letter_iter;
            if(!original_probabilities.count(letter))
                throw exception("Missing original probability for letter '%1%'.") % letter;

            const Real original_probability = original_probabilities.at(letter);
            if(original_probability < 0 || original_probability > 1)
                throw exception("Invalid original probability for letter '%1%'.") % letter;
            total_original_probability += original_probability;
        }
        if(std::abs(total_original_probability - 1) > 1e-5)
            throw exception("Total original probability = %1% is not consistent with 1.")
                % total_original_probability;
    }

    const std::string& GetName() const { return name; }
    const Alphabetum& GetAlphabet() const { return alphabet; }
    Real GetEntropy() const { return entropy; }
    Integer GetOriginalCounts() const { return original_counts; }

    Real GetOriginalProbability(const Letter& letter) const
    {
        CheckLetter(letter);
        return original_probabilities.at(letter);
    }

    Real GetOriginalFrequency(const Letter& letter) const
    {
        return GetOriginalProbability(letter) * GetOriginalCounts();
    }

    const HuffmanCode& GetHuffmanCode(const Letter& letter) const
    {
        CheckLetter(letter);
        return huffman_table.left.at(letter);
    }

    bool GetLetterFromHuffmanCode(const HuffmanCode& code, Letter& letter) const
    {
        if(!huffman_table.right.count(code)) return false;
        letter = huffman_table.right.at(code);
        return true;
    }

    void Write(std::ostream& os) const
    {
        static const int width = 20, header_width = 30;
        os << std::left << std::setprecision(5) << std::scientific << GetName() << "\n"
           << std::setw(header_width) << "number_of_letters " << GetAlphabet().size() << "\n"
           << std::setw(header_width) << "alphabet_entropy " << GetEntropy() << "\n"
           << std::setw(header_width) << "original_number_of_counts " << GetOriginalCounts() << "\n"
           << std::setw(width) << "Letter" << std::setw(width) << "Orig_probability"
           << std::setw(width) << "Huffman_nbits" << std::setw(width) << "Huffman_code" << "\n";
        for(const Letter& letter : GetAlphabet()) {
            const Real original_probability = GetOriginalProbability(letter);
            const HuffmanCode huffman_code = GetHuffmanCode(letter);

            os << std::setw(width) << letter << std::setw(width) << original_probability
               << std::setw(width) << huffman_code.NumberOfBits() << std::setw(width) << huffman_code << "\n";
        }
    }

    static AlphabetStatistics<Letter> Read(std::istream& is)
    {
        Alphabetum alphabet;
        LetterProbabilityMap original_probabilities;
        HuffmanTable huffman_table;
        std::string name;
        do {
            std::getline(is, name);
            if(name.size() && *name.rbegin() == '\r')
                name.erase(name.length() - 1);
            if(name.size() >= 3 && (unsigned char)(name.at(0)) == 0xEF && (unsigned char)(name.at(1)) == 0xBB
                    && (unsigned char)(name.at(2)) == 0xBF)
                name.erase(0, 3);
            boost::trim(name);
        } while(!name.size());
        const size_t n_letters = ReadParam<size_t>(is).second;
        const Real entropy = ReadParam<Real>(is).second;
        const Integer n_counts = ReadParam<Integer>(is).second;
        std::string table_header;
        std::getline(is, table_header);
        std::getline(is, table_header);
        for(size_t n = 0; n < n_letters; ++n) {
            Letter letter;
            Real original_probability;
            size_t huffman_nbits;
            HuffmanCode huffman_code;
            is >> letter >> original_probability >> huffman_nbits >> huffman_code;
            if(alphabet.count(letter))
                throw exception("Letter '%1' already defined.") % letter;
            alphabet.insert(letter);
            original_probabilities[letter] = original_probability;
            huffman_table.insert(typename HuffmanTable::value_type(letter, huffman_code));
        }

        return AlphabetStatistics<Letter>(name, alphabet, n_counts, original_probabilities, entropy, huffman_table);
    }

private:
    void CheckLetter(const Letter& letter) const
    {
        if(!alphabet.count(letter))
            throw exception("Letter '%1%' not present in the alphabet.") % letter;
    }

    template<typename Value>
    static std::pair<std::string, Value> ReadParam(std::istream& is)
    {
        std::string param_name;
        Value param_value;
        is >> param_name >> param_value;
        return std::make_pair(param_name, param_value);
    }

private:
    std::string name;
    Alphabetum alphabet;
    Integer original_counts;
    LetterProbabilityMap original_probabilities;
    Real entropy;
    HuffmanTable huffman_table;
};

template<typename Letter>
std::ostream& operator<<(std::ostream& os, const AlphabetStatistics<Letter>& stat)
{
    stat.Write(os);
    return os;
}

} // namespace pixel_studies
