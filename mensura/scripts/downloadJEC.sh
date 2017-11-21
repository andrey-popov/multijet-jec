#!/bin/bash

# This script downloads JEC tarballs from the official repository, unpacks them, deletes unneeded
# files, and copies remaining ones into the current directory.

set -e


dirSource=`pwd`
version="1"
blocks=("Summer16_07Aug2017BCD_V${version}_DATA" "Summer16_07Aug2017EF_V${version}_DATA" \
    "Summer16_07Aug2017G_V${version}_DATA" "Summer16_07Aug2017H_V${version}_DATA" \
    "Summer16_07Aug2017_V1_MC")

# Temporary directory to store intermediate files
dirTemp=`mktemp -d`
cd $dirTemp
echo $dirTemp


# Download and unpack all data
dirUnpacked=unpacked
mkdir $dirUnpacked

for block in ${blocks[*]}
do
    wget https://github.com/cms-jet/JECDatabase/raw/master/tarballs/${block}.tar.gz
    tar -xf ${block}.tar.gz
    
    if [ -d textFiles/${block} ]; then
        mv textFiles/${block}/* $dirUnpacked/
    else
        mv ${block}/* $dirUnpacked/
    fi
done


# Remove all files that are not needed
cd $dirUnpacked
find . -not -name "*AK4PFchs*" -delete
find . -name "*_DATA_Uncertainty*" -delete
find . -name "*DataMcSF*" -delete
find . -name "*_UncertaintySources_*" -delete

chmod 644 *.txt


# Move remaining files to the original current directory and remove temporary directory
mv *.txt $dirSource
cd $dirSource
rm -r $dirTemp
