# Configuration to test DictionaryBuilder class.
# This file is part of https://github.com/kandrosov/OnChipDataCompression.

import re
import importlib
import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing('analysis')
options.register('dictionaries', 'dictionaries.txt', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "Output file with dictionaries.")

options.parseArguments()

process = cms.Process('test')
process.options = cms.untracked.PSet()
process.options.wantSummary = cms.untracked.bool(False)
process.options.numberOfThreads = cms.untracked.uint32(4)
process.options.numberOfStreams = cms.untracked.uint32(0)

process.source = cms.Source('PoolSource', fileNames = cms.untracked.vstring(options.inputFiles))
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )

process.testDictionaryBuilder = cms.EDAnalyzer('TestDictionaryBuilder',
    outputFile = cms.string(options.dictionaries),
    pixelDigis = cms.InputTag('simSiPixelDigis', 'Pixel', 'HLT')
)
process.p = cms.Path(process.testDictionaryBuilder)
