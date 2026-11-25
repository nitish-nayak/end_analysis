// Copies an L1 file (l1tree/MakeL1Tree.C) keeping only the timeslices that lie entirely inside the good
// WR sync intervals of wrs/make_sync_intervals.py: [utc_s + utc_ns, + frame_ns] within one interval.
// The l1 tree has utc_ns; frames get it from the l1 windows of the same timeslice, and a timeslice
// without any window must have its whole UTC second inside an interval.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "TFile.h"
#include "TParameter.h"
#include "TTree.h"

typedef std::pair<int, int> TimesliceKey; // run, frame

std::vector<std::pair<double, double>> ReadSyncIntervals(const std::string& file)
{
  std::vector<std::pair<double, double>> out;
  std::ifstream in(file);
  std::string line;
  while(std::getline(in, line)){
    if(line.empty() || line[0] == '#' || line.rfind("utc_start", 0) == 0) continue;
    std::replace(line.begin(), line.end(), ',', ' ');
    std::istringstream s(line);
    double a, b;
    if(s >> a >> b) out.push_back(std::make_pair(a, b));
  }
  std::sort(out.begin(), out.end());
  return out;
}

// [start, start + length] inside one interval; times in s, compared in ns from the UTC second
bool InSync(const std::vector<std::pair<double, double>>& intervals, unsigned int utc_s, double start_ns, double length_ns)
{
  auto it = std::upper_bound(intervals.begin(), intervals.end(), std::make_pair((double)utc_s + start_ns * 1e-9, 1e30));
  if(it == intervals.begin()) return false;
  --it;
  return (it->first - utc_s) * 1e9 <= start_ns + 1 && (it->second - utc_s) * 1e9 >= start_ns + length_ns - 1;
}

void FilterL1Sync(std::string input, std::string output, std::string sync_file)
{
  const std::vector<std::pair<double, double>> intervals = ReadSyncIntervals(sync_file);
  if(intervals.empty()){
    std::cerr << "No intervals in " << sync_file << "! Exiting!" << std::endl;
    std::exit(1);
  }
  TFile* fin = TFile::Open(input.c_str(), "read");
  TTree* l1 = fin ? (TTree*)fin->Get("l1") : nullptr;
  TTree* frames = fin ? (TTree*)fin->Get("frames") : nullptr;
  if(!l1 || !frames){
    std::cerr << "No l1 and frames trees in " << input << "! Exiting!" << std::endl;
    std::exit(1);
  }

  // start of each timeslice, from the l1 windows
  int run, frame;
  unsigned int utc_s, utc_ns;
  double frame_ns;
  std::map<TimesliceKey, unsigned int> start_ns;
  l1->SetBranchStatus("*", 0);
  for(const char* b : {"run", "frame", "utc_ns"}) l1->SetBranchStatus(b, 1);
  l1->SetBranchAddress("run", &run);
  l1->SetBranchAddress("frame", &frame);
  l1->SetBranchAddress("utc_ns", &utc_ns);
  for(Long64_t i = 0; i < l1->GetEntries(); i++){
    l1->GetEntry(i);
    start_ns[TimesliceKey(run, frame)] = utc_ns;
  }
  l1->SetBranchStatus("*", 1);

  // timeslices in sync, decided on the frames (they give the length)
  std::map<TimesliceKey, bool> good;
  frames->SetBranchAddress("run", &run);
  frames->SetBranchAddress("frame", &frame);
  frames->SetBranchAddress("utc_s", &utc_s);
  frames->SetBranchAddress("frame_ns", &frame_ns);
  Long64_t n_no_window = 0;
  for(Long64_t i = 0; i < frames->GetEntries(); i++){
    frames->GetEntry(i);
    const TimesliceKey key(run, frame);
    if(good.count(key)) continue;
    auto it = start_ns.find(key);
    if(it == start_ns.end()) n_no_window++;
    good[key] = (it != start_ns.end()) ? InSync(intervals, utc_s, it->second, frame_ns) : InSync(intervals, utc_s, 0, 1e9);
  }

  TFile fout(output.c_str(), "RECREATE");
  TTree* l1_out = l1->CloneTree(0);
  TTree* frames_out = frames->CloneTree(0);
  l1->SetBranchAddress("run", &run);
  l1->SetBranchAddress("frame", &frame);
  for(Long64_t i = 0; i < l1->GetEntries(); i++){
    l1->GetEntry(i);
    auto it = good.find(TimesliceKey(run, frame));
    if(it != good.end() && it->second) l1_out->Fill();
  }
  for(Long64_t i = 0; i < frames->GetEntries(); i++){
    frames->GetEntry(i);
    if(good[TimesliceKey(run, frame)]) frames_out->Fill();
  }
  for(const char* p : {"window_ns", "min_m"}){
    TObject* o = fin->Get(p);
    if(o) o->Write(p);
  }
  fout.Write();

  int n_good = 0;
  for(auto& g : good) n_good += g.second;
  std::cout << "Timeslices in good sync : " << n_good << " / " << good.size()
            << " (" << n_no_window << " without L1 windows, judged by their whole second)" << std::endl;
  std::cout << "l1 : " << l1_out->GetEntries() << " / " << l1->GetEntries()
            << ", frames : " << frames_out->GetEntries() << " / " << frames->GetEntries() << std::endl;
  fout.Close();
  fin->Close();
  std::cout << "Wrote " << output << std::endl;
}
