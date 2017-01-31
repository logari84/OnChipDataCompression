# OnChipDataCompression
Code to study on-chip data compression algorithms for CMS Phase 2 Upgrade.
## How to install

```shell
cmsrel CMSSW_9_0_0_pre2
cd CMSSW_9_0_0_pre2/src
cmsenv
git clone git@github.com:kandrosov/OnChipDataCompression.git
scram b -j4
```

## How to run tests

```shell
cmsRun OnChipDataCompression/Algorithms/test/TestDictionaryBuilder.py maxEvents=10 inputFiles=file:DIGI_events.root
cmsRun OnChipDataCompression/Algorithms/test/TestChipDataEncoder.py maxEvents=10 inputFiles=file:DIGI_events.root dictionaries=dictionaries.txt
```
