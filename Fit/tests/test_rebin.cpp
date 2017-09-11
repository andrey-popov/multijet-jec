/**
 * A unit test for rebinning with interpolation.
 */


#include <Rebin.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>


using namespace std;


void printResult(bool pass)
{
    if (pass)
        cout << "\e[1;32mTest passed.\e[0m";
    else
        cout << "\e[1;31mTest failed.\e[0m";
    
    cout << endl;
}


ostream &operator<<(ostream &os, std::vector<double> const &v)
{
    os << "[";
    
    for (unsigned i = 0; i < v.size() - 1; ++i)
        os << v[i] << ", ";
    
    if (v.size() > 0)
        os << v[v.size() - 1];
    
    os << "]";
    return os;
}


void printBinMap(BinMap const &binMap, unsigned offset=2)
{
    string offsetStr;
    offsetStr.append(offset, ' ');
    
    for (auto const &m: binMap)
    {
        auto const &r = m.second;
        cout << offsetStr << m.first << " -> (" << r[0].index << " * " << r[0].frac << ") ... (" <<
          r[1].index << " * " << r[1].frac << ")\n";
    }
}


bool checkEntry(BinMap const &binMap, unsigned targetIndex, unsigned sourceIndex1,
  double sourceWeight1, unsigned sourceIndex2, double sourceWeight2)
{
    auto const &m = binMap.at(targetIndex);
    return (m[0].index == sourceIndex1 and std::abs(m[0].frac - sourceWeight1) < 1e-7 and
      m[1].index == sourceIndex2 and std::abs(m[1].frac - sourceWeight2) < 1e-7);
}


int main()
{
    bool failure = false;
    vector<double> source, target;
    
    
    cout << "Attempt an extrapolation. An error is expected.\n";
    bool exceptionCaught = false;
    source = {0., 1.};
    target = {-1, 0.5};
    
    try
    {
        mapBinning(source, target);
    }
    catch (std::logic_error const &)
    {
        cout << "  Exception of appropriate type detected.\n";
        exceptionCaught = true;
    }
    catch (...)
    {
        cout << "  Exception of unexpected type detected.\n";
    }
    printResult(exceptionCaught);
    failure |= not exceptionCaught;
    
    
    source = {1., 2., 3.};
    target = source;
    cout << "\nRebin " << source << " -> " << target << ":\n";
    BinMap res = mapBinning(source, target);
    printBinMap(res);
    bool status = (checkEntry(res, 0, 0, 1., 0, 0.) and checkEntry(res, 1, 1, 1., 1, 0.) and
      checkEntry(res, 2, 2, 1., 2, 0.) and checkEntry(res, 3, 3, 1., 3, 0.) and res.size() == 4);
    printResult(status);
    failure |= not status;
    
    
    source = {1., 2., 3.};
    target = {1., 3.};
    cout << "\nRebin " << source << " -> " << target << ":\n";
    res = mapBinning(source, target);
    printBinMap(res);
    status = (checkEntry(res, 0, 0, 1., 0, 0.) and checkEntry(res, 1, 1, 1., 2, 1.) and
      checkEntry(res, 2, 3, 1., 3, 0.) and res.size() == 3);
    printResult(status);
    failure |= not status;
    
    
    source = {1., 2., 3., 4., 5.};
    target = {1., 3., 5.};
    cout << "\nRebin " << source << " -> " << target << ":\n";
    res = mapBinning(source, target);
    printBinMap(res);
    status = (checkEntry(res, 0, 0, 1., 0, 0.) and checkEntry(res, 1, 1, 1., 2, 1.) and
      checkEntry(res, 2, 3, 1., 4, 1.) and checkEntry(res, 3, 5, 1., 5, 0.) and res.size() == 4);
    printResult(status);
    failure |= not status;
    
    
    source = {0., 0.25, 0.5, 0.75, 1.};
    target = {0., 1./3, 2./3, 1.};
    cout << "\nRebin " << source << " -> " << target << ":\n";
    res = mapBinning(source, target);
    printBinMap(res);
    status = (checkEntry(res, 0, 0, 1., 0, 0.) and checkEntry(res, 1, 1, 1., 2, 1./3) and
      checkEntry(res, 2, 2, 2./3, 3, 2./3) and checkEntry(res, 3, 3, 1./3, 4, 1.) and
      checkEntry(res, 4, 5, 1., 5, 0.) and res.size() == 5);
    printResult(status);
    failure |= not status;
    
    
    cout << endl;
    
    if (not failure)
    {
        cout << "\e[1;32mAll tests passed.\e[0m\n";
        return EXIT_SUCCESS;
    }
    else
    {
        cout << "\e[1;31mSome tests failed.\e[0m\n";
        return EXIT_FAILURE;
    }
}
