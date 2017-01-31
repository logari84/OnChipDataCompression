/*! Test for ChipDataEncoder class.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <TH1.h>
#include <TFile.h>
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "DataFormats/Common/interface/DetSetVector.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/SiPixelDigi/interface/PixelDigi.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "OnChipDataCompression/Algorithms/interface/ChipDataEncoder.h"

class TestChipDataEncoder : public edm::EDAnalyzer {
public:
    using Encoder = pixel_studies::ChipDataEncoder;
    using EncoderPtr = std::shared_ptr<Encoder>;
    using EncoderMap = std::map<std::string, EncoderPtr>;
    using EncoderFormat = pixel_studies::EncoderFormat;
    using Hist = TH1D;
    using HistPtr = std::shared_ptr<TH1D>;
    using HistMap = std::map<std::string, HistPtr>;
    using PixelDigiCollection = edm::DetSetVector<PixelDigi>;

    TestChipDataEncoder(const edm::ParameterSet& cfg) :
        dictionaries_file(cfg.getParameter<std::string>("dictionaries")),
        pixelDigis_token(consumes<PixelDigiCollection>(cfg.getParameter<edm::InputTag>("pixelDigis"))),
        chip_layout(400, 400, 1, 1), readout_unit_layout(2, 2)
    {
        using namespace pixel_studies;
        encoders["SinglePixel"] = std::make_shared<Encoder>(EncoderFormat::SinglePixel, chip_layout,
                                                            readout_unit_layout, 15);
        encoders["Region"] = std::make_shared<Encoder>(EncoderFormat::Region, chip_layout, readout_unit_layout, 15);
        encoders["RegionWithCompressedAdc"] = std::make_shared<Encoder>(
                    EncoderFormat::RegionWithCompressedAdc, chip_layout, readout_unit_layout, 15,
                    Ordering::ByRegionByColumn, dictionaries_file);
        encoders["Delta"] = std::make_shared<Encoder>(EncoderFormat::Delta, chip_layout, readout_unit_layout, 15,
                                                      Ordering::ByRegionByColumn, dictionaries_file);
        for(const auto& encoder_entry : encoders) {
            const std::string& name = "BitsPerChip_" + encoder_entry.first;
            histograms[name] = std::make_shared<Hist>(name.c_str(), name.c_str(), 12800, -0.5, 12799.5);
        }
    }

    virtual void analyze(const edm::Event& event, const edm::EventSetup& /*setup*/) override
    {
        using namespace pixel_studies;
        edm::Handle<PixelDigiCollection> pixelDigis;
        event.getByToken(pixelDigis_token, pixelDigis);
        for(const auto& detector : *pixelDigis) {
            // Here one should select only detectors that belongs to the same area for which dictionaries were built.
            // Moreover, module should be splet into chips.
            Chip chip(chip_layout);
            for(const PixelDigi& digi : detector) {
                const Pixel pixel(digi.row(), digi.column());
                const Adc adc(digi.adc() - 1);
                if(chip_layout.IsPixelInside(pixel))
                    chip.AddPixel(pixel, adc);
            }
            for(const auto& encoder_entry : encoders) {
                const auto& package = encoder_entry.second->Encode(chip);
                const Chip decoded_chip = encoder_entry.second->Decode(package);
                if(decoded_chip != chip) {
                    std::cout << "Module id: " << detector.id << ". PackageMaker: " << encoder_entry.first << std::endl;
                    chip.HasSamePixels(decoded_chip, &std::cerr);
                    throw pixel_studies::exception("invalid encoding-decoding");
                }
                const std::string& hist_name = "BitsPerChip_" + encoder_entry.first;
                FillHistogram(hist_name, package.size());
            }
        }
    }

    virtual void endJob()
    {
        auto& file = edm::Service<TFileService>()->file();
        for(const auto& hist_entry : histograms)
            file.WriteTObject(hist_entry.second.get(), hist_entry.first.c_str());
    }

private:
    void FillHistogram(const std::string& name, double value)
    {
        std::unique_lock<std::mutex> lock(mutex);
        histograms.at(name)->Fill(value);
    }

private:
    std::mutex mutex;
    std::string dictionaries_file;
    edm::EDGetTokenT<PixelDigiCollection> pixelDigis_token;
    pixel_studies::MultiRegionLayout chip_layout;
    pixel_studies::RegionLayout readout_unit_layout;

    EncoderMap encoders;
    HistMap histograms;
};

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(TestChipDataEncoder);
