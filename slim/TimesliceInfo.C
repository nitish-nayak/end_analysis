// Prints the number of L0 timeslices in a file and the UTC seconds of the first and last one.

#include <iostream>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"

#include "km3net-dataformat/online/JDAQSuperFrame.hh"

R__LOAD_LIBRARY(libKM3NeTROOT.so)

void TimesliceInfo(std::string input)
{
  TFile* fin = TFile::Open(input.c_str(), "read");
  TTree* l0 = fin ? (TTree*)fin->Get("KM3NET_TIMESLICE_L0") : nullptr;
  if(!l0){
    std::cerr << "No KM3NET_TIMESLICE_L0 tree in " << input << "! Exiting!" << std::endl;
    std::exit(1);
  }
  l0->SetMakeClass(1);
  std::vector<KM3NETDAQ::JDAQSuperFrame> superframes;
  TBranch* branch = l0->GetBranch("vector<KM3NETDAQ::JDAQSuperFrame>");
  branch->SetAddress(&superframes);

  const Long64_t n = l0->GetEntries();
  branch->GetEntry(0);
  const unsigned int first = superframes.at(0).getTimesliceStart().getUTCseconds();
  branch->GetEntry(n - 1);
  const unsigned int last = superframes.at(0).getTimesliceStart().getUTCseconds();

  std::cout << "timeslices " << n << std::endl;
  std::cout << "first_utc_s " << first << std::endl;
  std::cout << "last_utc_s " << last << std::endl;
  std::cout << "mb_per_timeslice " << l0->GetZipBytes() / 1e6 / n << std::endl;
  fin->Close();
}
