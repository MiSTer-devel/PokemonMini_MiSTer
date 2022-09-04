#!/bin/bash
rm -rf db
rm -rf incremental_db
rm -rf output_files
rm -f build_id.v
rm -f c5_pin_model_dump.txt
rm -f PLLJ_PLLSPE_INFO.txt
find . -name "*.qws" -type f -delete
find . -name "*.ppf" -type f -delete
find . -name "*.ddb" -type f -delete
find . -name "*.csv" -type f -delete
find . -name "*.cmp" -type f -delete
find . -name "*.sip" -type f -delete
find . -name "*.spd" -type f -delete
find . -name "*.bsf" -type f -delete
find . -name "*.f" -type f -delete
find . -name "*.sopcinfo" -type f -delete
find . -name "*.xml" -type f -delete
for d in sys/*_sim/; do rm -rf $d; done
for d in rtl/*_sim/; do rm -rf $d; done
rm -f *.cdf
rm -f *.rpt
