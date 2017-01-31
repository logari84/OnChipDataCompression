/*! The class that represents a collection of AlphabetStatistics.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <fstream>
#include "AlphabetStatistics.h"
#include "Pixel.h"

namespace pixel_studies {

enum class AlphabetType { Adc, ActiveAdc, DeltaRow, DeltaColumn, DeltaRowColumn };

template<typename _Letter>
class AlphabetStatisticsCollection {
public:
    using Statistics = AlphabetStatistics<_Letter>;
    using StatisticsPtr = std::shared_ptr<Statistics>;
    using StatisticsMap = std::map<std::string, StatisticsPtr>;

public:
    AlphabetStatisticsCollection(const std::string& file_name)
    {
        std::ifstream f(file_name);
        f.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
        try {
            while(true) {
                StatisticsPtr stat;
                stat = StatisticsPtr(new Statistics(Statistics::Read(f)));

                if(statistics.count(stat->GetName()))
                    throw exception("Alphabet statistics with name '%1%' is already defined.") % stat->GetName();
                statistics[stat->GetName()] = stat;
            }
        } catch(std::ifstream::failure& e) {
            if(!f.eof())
                throw exception("Error while reading alphabet statistics file: %1%") % e.what();
        }
    }

    size_t count(const std::string& name) const { return statistics.count(name); }

    StatisticsPtr at(const std::string& name) const
    {
        if(!statistics.count(name))
            throw exception("Alphabet statistics '%1%' not found.") % name;
        return statistics.at(name);
    }

    StatisticsPtr at(AlphabetType alphabet_type) const
    {
        static const std::map<AlphabetType, std::string> alphabetTypeNames = {
            { AlphabetType::Adc, "all_adc" },
            { AlphabetType::ActiveAdc, "active_adc" },
            { AlphabetType::DeltaRowColumn, "delta_row_column" }
        };
        return at(alphabetTypeNames.at(alphabet_type));
    }

private:
    StatisticsMap statistics;
};

} // namespace pixel_studies
