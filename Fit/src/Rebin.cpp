#include <Rebin.hpp>

#include <sstream>
#include <stdexcept>


BinMap mapBinning(std::vector<double> const &source, std::vector<double> const &target)
{
    // Verify that the full range of the target binning is containted within the range of the
    //source binning
    if (target[0] < source[0] or target[target.size() - 1] > source[source.size() - 1])
    {
        std::ostringstream message;
        message << "mapBinning: Range of target binning (" << target[0] << ", " <<
          target[target.size() - 1] << ") is not included in the range of source binning (" <<
          source[0] << ", " << source[source.size() - 1] << ").";
        throw std::logic_error(message.str());
    }
    
    
    // Perform a matching from the target binning to the source one. Each edge of the target
    //binning is represented by the index of the  bin of the source binning that contain this edge
    //and its position within that bin, which is expressed in terms of the bin width. Bins are
    //numbered by the indices of their lower boundaries. The underflow bin has index -1.
    std::vector<FracBin> matchedEdges;
    matchedEdges.reserve(target.size());
    int curSrcBin = -1;
    
    for (auto const &x: target)
    {
        // Scroll to the bin of the source binning that contains value x
        while (curSrcBin < int(source.size()) - 1 and source[curSrcBin + 1] < x)
            ++curSrcBin;
        
        // Find the relative position inside the source bin. The two  special cases can only occur
        //when boundaries of the ranges of the two binnings are approximately equal. The relative
        //positions are set under this assumption.
        double relPos;
        
        if (curSrcBin == -1)
            relPos = 1.;
        else if (curSrcBin == int(source.size()) - 1)
            relPos = 0.;
        else
        {
            double const srcBinStart = source[curSrcBin];
            double const srcBinWidth = source[curSrcBin + 1] - srcBinStart;
            relPos = (x - srcBinStart) / srcBinWidth;
        }
        
        matchedEdges.emplace_back(FracBin{unsigned(curSrcBin), relPos});
    }
    
    
    // Turn the collection of matched edges into a collection of ranges of bins of the source
    //binning. Such a range is built for each target bin.
    std::vector<FracBin> boundaries;
    boundaries.reserve(2 * target.size() + 2);
    unsigned srcBin;
    double fraction;
    
    // The underflow bin for the source binning is always included in the underflow of the target
    boundaries.emplace_back(FracBin{unsigned(-1), 1.});
    
    
    for (unsigned i = 0; i < matchedEdges.size(); ++i)
    {
        srcBin = matchedEdges[i].index;
        double relPos = matchedEdges[i].frac;
        
        // A the moment the algorithm is inside a bin range. Find the closing boundary for this
        //range. If the closing boundary (which is from the source binning) is compatible with the
        //upper edge of the current target bin, treat them as equal. If the relative position is
        //compatible with 0., interpret it as a relative position of 1. within the previous source
        //bin.
        double const tolerance = 1e-7;
        
        if (relPos > 1. - tolerance)
            relPos = 1.;
        
        if (relPos < tolerance and srcBin != unsigned(-1))
        {
            --srcBin;
            relPos = 1.;
        }
        
        
        fraction = relPos;
        
        if (srcBin == boundaries[boundaries.size() - 1].index)
        {
            // If this closing boundary corresponds to the same source bin as the previous
            //(opening) boundary, set its bin fraction to zero in order to simplify iterating over
            //produced bin ranges
            fraction = 0.;
        }
        
        boundaries.emplace_back(FracBin{srcBin, fraction});
        
        
        // Now construct an opening boundary. If the relative position is 1., interpret it as a
        //relative position of 0. within the next source bin in order to avoid bins with an
        //inclusion fraction of zero.
        if (relPos == 1.)
        {
            srcBin += 1;
            relPos = 0.;
        }
        
        if (i < matchedEdges.size() - 1 and matchedEdges[i + 1].index == srcBin)
        {
            // There is more than one target bin edge that is included in the current source bin
            fraction = matchedEdges[i + 1].frac - relPos;
        }
        else
            fraction = 1. - relPos;
        
        boundaries.emplace_back(FracBin{srcBin, fraction});
    }
    
    
    // The last closing boundary is the overflow bin of the source binning. As done for other
    //closing boundaries, set the inclusion fraction to zero when it corresponds to the same source
    //bin as the last opening boundary.
    srcBin = source.size() - 1;
    fraction = (srcBin == boundaries[boundaries.size() - 1].index) ? 0. : 1.;
    boundaries.emplace_back(FracBin{srcBin, fraction});
    
    
    // Convert the collection of constructed boundaries into a bin map. Switch to the bin numbering
    //convention of ROOT, where the underflow bin gets an index of zero.
    BinMap binMap;
    
    for (unsigned targetBin = 0; targetBin < target.size() + 1; ++targetBin)
    {
        auto const &start = boundaries[targetBin * 2];
        auto const &end = boundaries[targetBin * 2 + 1];
        
        std::array<FracBin, 2> range;
        range[0] = {start.index + 1, start.frac};
        range[1] = {end.index + 1, end.frac};
        
        binMap[targetBin] = range;
    }
    
    return binMap;
}
