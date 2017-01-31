/*! The class that collects statistics for a given alphabet.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include "AlphabetStatistics.h"
#include "HuffmanTree.h"

namespace pixel_studies {

template<typename _Letter>
class AlphabetStatisticsProducer {
public:
    using Letter = _Letter;
    using Statistics = AlphabetStatistics<Letter>;
    using Alphabetum = typename Statistics::Alphabetum;
    using Integer = typename Statistics::Integer;
    using Real = typename Statistics::Real;
    using LetterProbabilityMap = typename Statistics::LetterProbabilityMap;
    using LetterFrequencyMap = std::map<Letter, Integer>;
    using LetterFrequencyPair = std::pair<Letter, Integer>;
    using LetterFrequencyVector = std::vector<LetterFrequencyPair>;
    using StatisticsPtr = std::shared_ptr<Statistics>;

    explicit AlphabetStatisticsProducer(const std::string& _name) :
        name(_name), n_counts(0) {}

    AlphabetStatisticsProducer(const AlphabetStatisticsProducer<Letter>& other) :
        name(other.name), n_counts(other.n_counts), letter_frequencies(other.letter_frequencies) {}

    AlphabetStatisticsProducer(AlphabetStatisticsProducer<Letter>&& other) :
        name(other.name), n_counts(other.n_counts), letter_frequencies(other.letter_frequencies) {}

    template<typename LetterCollection>
    AlphabetStatisticsProducer(const std::string& _name, const LetterCollection* alphabet) :
        AlphabetStatisticsProducer(_name)
    {
        if(alphabet) {
            for(const Letter& letter : *alphabet)
                letter_frequencies[letter] = 0;
        }
    }

    bool IntegerLimitIsReached() const { return n_counts == std::numeric_limits<Integer>::max(); }
    const std::string& GetName() const { return name; }
    size_t GetNumberOfLetters() const { return letter_frequencies.size(); }

    void AddCount(const Letter& letter)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if(IntegerLimitIsReached()) return;
        ++letter_frequencies[letter];
        ++n_counts;
    }

    StatisticsPtr Produce()
    {
        std::unique_lock<std::mutex> lock(mutex);
        std::cout << "Producing alphabet statistics for '" << GetName() << "'... ";

        const LetterFrequencyVector ordered_original_frequencies = GetFrequenceyOrderedLetters();
        LetterProbabilityMap original_probabilities;
        Real entropy = 0;
        for(size_t n = 0; n < ordered_original_frequencies.size(); ++n) {
            const Letter& letter = ordered_original_frequencies.at(n).first;
            const Integer original_freq = ordered_original_frequencies.at(n).second;
            const Real original_prob = Real(original_freq) / n_counts;
            original_probabilities[letter] = original_prob;
            if(original_prob)
                entropy -= original_prob * std::log2(original_prob);
        }
        std::cout << "entropy = " << entropy << ".\n";
        const HuffmanTree<Letter, Integer> huffman_tree(letter_frequencies);
        Alphabetum alphabet;
        boost::copy(letter_frequencies | boost::adaptors::map_keys, std::inserter(alphabet, alphabet.end()));
        return StatisticsPtr(new Statistics(name, alphabet, n_counts, original_probabilities, entropy,
                                            huffman_tree.GetTable()));
    }

    std::shared_ptr<AlphabetStatisticsProducer<Letter>> Reduce(size_t new_alphabet_size, const std::string& new_name,
                                                               const Letter& special_letter)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if(new_alphabet_size <= 1)
            throw exception("New alphabet size = %1% is too small.") % new_alphabet_size;
        if(letter_frequencies.count(special_letter))
            throw exception("Special letter '%1%' already present in the alphabet.") % special_letter;
        const LetterFrequencyVector ordered_letters = GetFrequenceyOrderedLetters();
        if(ordered_letters.size() <= new_alphabet_size)
            return std::make_shared<AlphabetStatisticsProducer<Letter>>(*this);

        std::shared_ptr<AlphabetStatisticsProducer<Letter>> reduced_producer(
                    new AlphabetStatisticsProducer<Letter>(new_name));
        reduced_producer->n_counts = n_counts;
        Integer count = 0;
        for(size_t n = 0; n < new_alphabet_size - 1; ++n) {
            const size_t index = ordered_letters.size() - n - 1;
            const Letter& letter = ordered_letters.at(index).first;
            const Integer frequency = ordered_letters.at(index).second;
            reduced_producer->letter_frequencies[letter] = frequency;
            count += frequency;
        }
        reduced_producer->letter_frequencies[special_letter] = n_counts - count;
        return reduced_producer;
    }

private:
    LetterFrequencyVector GetFrequenceyOrderedLetters() const
    {
        if(!n_counts)
            throw exception("Statistics is not available for '%1%'.") % GetName();
        if(IntegerLimitIsReached())
            std::cerr << "WARNING: Integer limit was reached while collecting statistics.\n";
        LetterFrequencyVector ordered_original_frequencies(letter_frequencies.begin(), letter_frequencies.end());
        std::sort(ordered_original_frequencies.begin(), ordered_original_frequencies.end(),
                  [](const LetterFrequencyPair& first, const LetterFrequencyPair& second) -> bool {
            if(first.second < second.second) return true;
            if(first.second > second.second) return false;
            return first.first > second.first;
        });
        return ordered_original_frequencies;
    }

private:
    std::mutex mutex;
    std::string name;
    Integer n_counts;
    LetterFrequencyMap letter_frequencies;
};

} // namespace pixel_studies
