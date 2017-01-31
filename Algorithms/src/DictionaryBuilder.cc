/*! The class that creates dictionaries for compression algorithms.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#include <fstream>
#include "../interface/DictionaryBuilder.h"

namespace pixel_studies {

DictionaryBuilder::Producer DictionaryBuilder::CreateProducer(const std::string& name, const Letter& begin,
                                                              const Letter& end)
{
    Alphabetum alphabetum;
    for(Letter letter = begin; letter != end; ++letter)
        alphabetum.insert(letter);
    return Producer(name, &alphabetum);
}

DictionaryBuilder::DictionaryBuilder(const MultiRegionLayout& _chip_layout, Ordering _ordering,
                                     const RegionLayout& _readout_unit_layout, size_t max_adc,
                                     size_t _max_alphabet_size) :
    chip_layout(_chip_layout), ordering(_ordering), readout_unit_layout(_readout_unit_layout),
    max_alphabet_size(_max_alphabet_size), all_adc_prod(CreateProducer("all_adc", 0, max_adc)),
    active_adc_prod(CreateProducer("active_adc", 1, max_adc)),
    delta_row_column_prod(CreateProducer("delta_row_column", 0, chip_layout.region_layout.GetNumberOfPixels()))
{
}

void DictionaryBuilder::AddChip(const Chip& original_chip)
{
    ChipPtr split_chip;
    const Chip* chip = nullptr;
    if(original_chip.GetMultiRegionLayout() == chip_layout) {
        chip = &original_chip;
    } else {
        split_chip = std::make_shared<Chip>(original_chip, chip_layout.n_region_rows, chip_layout.n_region_columns);
        chip = split_chip.get();
    }

    for(size_t n = 0; n < chip_layout.GetNumberOfRegions(); ++n) {
        if(!chip->IsRegionActive(n)) continue;
        const PixelMultiRegion pixel_region(chip->GetRegion(n), readout_unit_layout);
        const auto ordered_pixels = pixel_region.GetOrderedPixels(ordering);
        ProcessOrderedPixels(ordered_pixels);
        ProcessRegionBlocks(pixel_region);
    }
}

void DictionaryBuilder::ProcessOrderedPixels(const PixelWithAdcVector& ordered_pixels)
{
    const auto& layout = chip_layout.region_layout;
    Pixel previous_pixel(0, 0);
    for(const PixelAdcPair& pixel_with_adc : ordered_pixels) {
        const Pixel& pixel = pixel_with_adc.first;
        const Adc adc = pixel_with_adc.second;
        const auto delta_row = (pixel.row + layout.n_rows - previous_pixel.row) % layout.n_rows;
        const auto delta_column = (pixel.column + layout.n_columns - previous_pixel.column) % layout.n_columns;
        const auto delta_row_column = layout.GetPixelId(Pixel(delta_row, delta_column));
        active_adc_prod.AddCount(adc);
        delta_row_column_prod.AddCount(delta_row_column);
        previous_pixel = pixel;
    }
}

void DictionaryBuilder::ProcessRegionBlocks(const PixelMultiRegion& pixel_area)
{
    for(size_t n = 0; n < pixel_area.GetMultiRegionLayout().GetNumberOfRegions(); ++n) {
        if(!pixel_area.IsRegionActive(n)) continue;
        const PixelRegion& region = pixel_area.GetRegion(n);
        const RegionLayout& layout = region.GetRegionLayout();
        for(size_t row = 0; row < layout.n_rows; ++row) {
            for(size_t column = 0; column < layout.n_columns; ++column) {
                const Adc adc = region.GetAdc(row, column);
                all_adc_prod.AddCount(adc);
            }
        }
    }
}

void DictionaryBuilder::SaveDictionaries(const std::string& cfg_file_name)
{
    std::unique_lock<std::mutex> lock(mutex);
    try {
        std::ofstream cfg(cfg_file_name);
        cfg.exceptions(std::ofstream::badbit | std::ofstream::failbit);
        SaveStatistics(all_adc_prod, cfg, false);
        SaveStatistics(active_adc_prod, cfg, false);
        SaveStatistics(delta_row_column_prod, cfg, true);
    } catch(std::ofstream::failure&) {
        throw exception("Error while saving dictionaries into '%1%'") % cfg_file_name;
    }
}

void DictionaryBuilder::SaveStatistics(Producer& producer, std::ostream& os, bool reduce) const
{
    ProducerPtr reduced_producer;
    Producer* active_producer = &producer;
    if(reduce && producer.GetNumberOfLetters() > max_alphabet_size) {
        reduced_producer = producer.Reduce(max_alphabet_size, producer.GetName(), -1);
        active_producer = reduced_producer.get();
    }
    const auto stat = active_producer->Produce();
    os << *stat << std::endl;
}

} // namespace pixel_studies
