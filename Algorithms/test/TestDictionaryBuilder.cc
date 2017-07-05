/*! Test for DictionaryBuilder class.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#include <iostream>
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/Event.h"
#include "DataFormats/Common/interface/DetSetVector.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/SiPixelDigi/interface/PixelDigi.h"
#include "OnChipDataCompression/Algorithms/interface/DictionaryBuilder.h"

class TestDictionaryBuilder : public edm::EDAnalyzer {
public:
    using Builder = pixel_studies::DictionaryBuilder;
    using BuilderPtr = std::shared_ptr<Builder>;
    using PixelDigiCollection = edm::DetSetVector<PixelDigi>;

    TestDictionaryBuilder(const edm::ParameterSet& cfg) :
        outputFile(cfg.getParameter<std::string>("outputFile")),
        pixelDigis_token(consumes<PixelDigiCollection>(cfg.getParameter<edm::InputTag>("pixelDigis"))),
        chip_layout(400, 400, 1, 4), readout_unit_layout(2, 2),
        builder(chip_layout, pixel_studies::Ordering::ByRegionByColumn, readout_unit_layout, 15, 32)
    {
    }

    virtual void analyze(const edm::Event& event, const edm::EventSetup& /*setup*/) override
    {
        using namespace pixel_studies;
        edm::Handle<PixelDigiCollection> pixelDigis;
        event.getByToken(pixelDigis_token, pixelDigis);
        for(const auto& detector : *pixelDigis) {
            const auto& detId = detector.detId();
            int layerId = 0, partId = -1;
            if(filterResult.detId.subdetId() == PixelSubdetector::PixelBarrel) {
                const PXBDetId detId(filterResult.detId);
                layerId = static_cast<int>(detId.layer());
                partId = 0;
            } else if(filterResult.detId.subdetId() == PixelSubdetector::PixelEndcap) {
                const PXFDetId detId(filterResult.detId);
                layerId = detId.disk();
                partId = 1;
                if(detId.side() == 2)
                    layerId *= -1;
                else if(detId.side() != 1)
                    throw std::runtime_error("Bad PXFDetId");
            } else {
                throw std::runtime_error("Bad DetId");
            }

            if(partId != 0 || layerId != 1) continue;
            Chip chip(chip_layout);
            for(const PixelDigi& digi : detector) {
                const Pixel pixel(digi.row(), digi.column());
                const Adc adc(digi.adc() - 1);
                if(chip_layout.IsPixelInside(pixel))
                    chip.AddPixel(pixel, adc);
            }
            builder.AddChip(chip);
        }
    }

    virtual void endJob()
    {
        builder.SaveDictionaries(outputFile);
    }

private:
    std::string outputFile;
    edm::EDGetTokenT<PixelDigiCollection> pixelDigis_token;
    pixel_studies::MultiRegionLayout chip_layout;
    pixel_studies::RegionLayout readout_unit_layout;
    Builder builder;
};

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(TestDictionaryBuilder);

