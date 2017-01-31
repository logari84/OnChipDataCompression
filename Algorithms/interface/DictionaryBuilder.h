/*! The class that creates dictionaries for compression algorithms.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include "Chip.h"
#include "AlphabetStatisticsProducer.h"

namespace pixel_studies {

class DictionaryBuilder {
public:
    using Letter = int;
    using Producer = AlphabetStatisticsProducer<Letter>;
    using Alphabetum = Producer::Alphabetum;
    using StatisticsPtr = typename Producer::StatisticsPtr;
    using ProducerPtr = std::shared_ptr<Producer>;

    DictionaryBuilder(const MultiRegionLayout& _chip_layout, Ordering _ordering,
                      const RegionLayout& _readout_unit_layout, size_t _max_adc, size_t _max_alphabet_size);
    void AddChip(const Chip& chip);
    void SaveDictionaries(const std::string& cfg_file_name);

private:
    static Producer CreateProducer(const std::string& name, const Letter& begin, const Letter& end);
    void ProcessOrderedPixels(const PixelWithAdcVector& ordered_pixels);
    void ProcessRegionBlocks(const PixelMultiRegion& pixel_area);
    void SaveStatistics(Producer& producer, std::ostream& os, bool reduce) const;

private:
    std::mutex mutex;
    const MultiRegionLayout chip_layout;
    const Ordering ordering;
    const RegionLayout readout_unit_layout;
    const size_t max_alphabet_size;
    Producer all_adc_prod, active_adc_prod, delta_row_column_prod;
};

} // namespace pixel_studies
