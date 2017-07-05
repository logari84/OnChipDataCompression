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
#include "DataFormats/SiPixelDetId/interface/PXBDetId.h"
#include "DataFormats/SiPixelDetId/interface/PXFDetId.h"

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
    using Package = pixel_studies::Package;

    TestChipDataEncoder(const edm::ParameterSet& cfg) :
        dictionaries_file(cfg.getParameter<std::string>("dictionaries")),
        pixelDigis_token(consumes<PixelDigiCollection>(cfg.getParameter<edm::InputTag>("pixelDigis"))),
        chip_layout(400, 400, 1, 4), readout_unit_layout(2, 2)
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
            CreateCommonHist("BitsPerChip_" + encoder_entry.first, 12800);
            CreateCommonHist("BitsPerItem_" + encoder_entry.first, 1280);
            CreateCommonHist("N_readoutClc_" + encoder_entry.first, 1000);
            CreateCommonHist("N_readoutActiveClc_" + encoder_entry.first, 1000);
            CreateCommonHist("N_readoutInactiveClc_" + encoder_entry.first, 1000);
            CreateCommonHist("Max_readoutQueue_" + encoder_entry.first, 1000);
            CreateCommonHist("ADC_" + encoder_entry.first, 16);
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
            const DetId detId(detector.detId());
            int layerId = 0, partId = -1;
            if(detId.subdetId() == PixelSubdetector::PixelBarrel) {
                const PXBDetId pxbDetId(detId);
                layerId = static_cast<int>(pxbDetId.layer());
                partId = 0;
            } else if(detId.subdetId() == PixelSubdetector::PixelEndcap) {
                const PXFDetId pxfDetId(detId);
                layerId = pxfDetId.disk();
                partId = 1;
                if(pxfDetId.side() == 2)
                    layerId *= -1;
                else if(pxfDetId.side() != 1)
                    throw std::runtime_error("Bad PXFDetId");
            } else {
                throw std::runtime_error("Bad DetId");
                }
            
            
            Chip chip(chip_layout);
            for(const PixelDigi& digi : detector) {
                const Pixel pixel(digi.row(), digi.column());
                const Adc adc(digi.adc() - 1);
                if(chip_layout.IsPixelInside(pixel))
                    chip.AddPixel(pixel, adc);
                FillHistogram("Max_readoutQueue_" + maker_name, adc);
            }
            for(const auto& encoder_entry : encoders) {
                const auto& package = encoder_entry.second->Encode(chip);
                const Chip decoded_chip = encoder_entry.second->Decode(package);
                if(decoded_chip != chip) {
                    std::cout << "Module id: " << detector.id << ". PackageMaker: " << encoder_entry.first << std::endl;
                    chip.HasSamePixels(decoded_chip, &std::cerr);
                    throw pixel_studies::exception("invalid encoding-decoding");
                }
                AnalyzePackage(encoder_entry.first, package);
            }
        }
    }

    virtual void endJob()
    {
        static const std::vector<double> quantiles = { 0.01, 0.001, 0.0001 };
        static const size_t first_column_width = 50, q_column_width = 15;
        static const std::string h_sep = " | ";
        static const size_t v_sep_width = first_column_width + h_sep.size()
                + quantiles.size() * (q_column_width + h_sep.size());
        static const std::string v_sep(v_sep_width, '-');

        std::cout << v_sep << "\n" << std::left << std::setw(first_column_width) << "Histogram name" << h_sep;
        for(size_t n = 0; n < quantiles.size(); ++n) {
            std::ostringstream ss;
            const double efficiency = (1. - quantiles.at(n)) * 100.;
            ss << efficiency << "% events";
            std::cout << std::setw(q_column_width) << ss.str() << h_sep;
        }
        std::cout << "\n" << v_sep << "\n";
        auto& file = edm::Service<TFileService>()->file();
        for(const auto& hist_entry : histograms) {
            std::cout << std::left << std::setw(first_column_width) << hist_entry.first << h_sep;
            for(size_t n = 0; n < quantiles.size(); ++n) {
                const double upper_limit = GetUpperLimit(hist_entry.first, quantiles.at(n));
                std::ostringstream ss;
                ss << "< " << upper_limit;
                std::cout << std::setw(q_column_width) << ss.str() << h_sep;
            }
            std::cout << "\n";

            file.WriteTObject(hist_entry.second.get(), hist_entry.first.c_str());
        }
        std::cout << v_sep << std::endl;
    }

private:
    void CreateCommonHist(const std::string& name, size_t n_bins)
    {
        histograms[name] = std::make_shared<Hist>(name.c_str(), name.c_str(), n_bins, -0.5, n_bins - 0.5);
    }

    void FillHistogram(const std::string& name, double value)
    {
        std::unique_lock<std::mutex> lock(mutex);
        histograms.at(name)->Fill(value);
    }

    void AnalyzePackage(const std::string& maker_name, const Package& package)
    {
        using PositionCollection = Package::PositionCollection;
        static const size_t out_size = 64;

        FillHistogram("BitsPerChip_" + maker_name, package.size());
        const PositionCollection& queue = package.readout_positions();
        size_t n_clc = 0, n_active_clc = 0, out_queue_pos = 0, prev_pos = 0, max_out_queue_pos = 0;
        for(size_t pos : queue) {
            const size_t item_size = pos - prev_pos;
            FillHistogram("BitsPerItem_" + maker_name, item_size);
            out_queue_pos += item_size;
            max_out_queue_pos = std::max(out_queue_pos, max_out_queue_pos);
            prev_pos = pos;
            if(out_queue_pos >= out_size) {
                out_queue_pos -= out_size;
                ++n_active_clc;
            }
            ++n_clc;
        }
        while(out_queue_pos) {
            out_queue_pos -= std::min(out_queue_pos, out_size);
            ++n_clc; ++n_active_clc;
        }
        FillHistogram("N_readoutClc_" + maker_name, n_clc);
        FillHistogram("N_readoutActiveClc_" + maker_name, n_active_clc);
        FillHistogram("N_readoutInactiveClc_" + maker_name, n_clc - n_active_clc);
        FillHistogram("Max_readoutQueue_" + maker_name, max_out_queue_pos);
    }

    double GetUpperLimit(const std::string& name, double quantile) const
    {
        const auto& hist = histograms.at(name);
        const int last_bin = hist->GetNbinsX() + 1;
        const double full_integral = hist->Integral(0, last_bin);
        const double q_integral = quantile * full_integral;
        int q_bin = last_bin;
        while(hist->Integral(q_bin, last_bin) < q_integral)
            --q_bin;
        return hist->GetBinLowEdge(q_bin);
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
