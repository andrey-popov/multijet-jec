/**
 * Provides tools to change a binning with an interpolation.
 */

#pragma once

#include <array>
#include <map>
#include <vector>


/**
 * \struct FracBin
 * \brief Auxiliary POD to describe a bin with an inclusion fraction
 * 
 * Used to describe bins that are partly included in a range.
 */
struct FracBin
{
    /// Index of the bin
    unsigned index;
    
    /// Included fraction of the bin
    double frac;
};


/// An alias for type returned by function mapBinning
using BinMap = std::map<unsigned, std::array<FracBin, 2>>;


/**
 * \brief Constructs a mapping from one binning to another
 * 
 * Constructs a mapping from the source binning to the target one. If bin edges do not align,
 * performs an interpolation. The full range of the target binning must be included in the range of
 * the source one (i.e. no extrapolation is performed). Both vectors must be sorted; this condition
 * is not verified. Normally the target binning is coarser so that source bins are merged, but this
 * is not required, and a single source bin can be mapped to multiple bins of the target binning.
 * 
 * Returns a map from indices of target bins to ranges of bins of the source binning. The ranges
 * are represented by pairs of FracBin objects, which give indices of boundary bins of the range
 * and their inclusion fractions. If both ends of the range have the same index (i.e. a single
 * source bin has been mapped into multiple target bins), the inclusion fraction for the upper
 * boundary is set to zero. Bins are numbered such that the underflow bin is assigned index 0.
 */
BinMap mapBinning(std::vector<double> const &source, std::vector<double> const &target);
