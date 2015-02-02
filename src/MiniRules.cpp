#include "MiniRules.h"
#include "VariantBamReader.h"
#include <regex>

using namespace std;
using namespace BamTools;

bool MiniRules::isValid(BamAlignment &a) {

  for (auto it : m_abstract_rules)
    if (it.isValid(a)) 
       return true; // it is includable in at least one. 
      
  return false;

}

// check whether a BamAlignment (or optionally it's mate) is overlapping the regions
// contained in these rules
bool MiniRules::isOverlapping(BamAlignment &a) {

  // if this is a whole genome rule, it overlaps
  if (m_whole_genome)
    return true;

  // TODO fix a.MatePosition + a.Length is using wrong length

  // check whether a read (or maybe its mate) hits a rule
  GenomicIntervalVector grv;
  if (m_tree.count(a.RefID) == 1) // check that we have a tree for this chr
    m_tree[a.RefID].findOverlapping(a.Position, a.Position + a.Length, grv);
  if (m_tree.count(a.MateRefID) == 1 && m_applies_to_mate) // check that we have a tree for this chr
    m_tree[a.MateRefID].findOverlapping (a.MatePosition, a.MatePosition + a.Length, grv);
  return grv.size() > 0;
  
}

// checks which rule a read applies to (using the hiearchy stored in m_regions).
// if a read does not satisfy a rule it is excluded.
string MiniRulesCollection::isValid(BamAlignment &a) {

  size_t which_region = 0;
  size_t which_rule = 0;
  
  // find out which rule it is a part of
  // lower number rules dominate

  for (auto it : m_regions) {
    which_rule = 0;
    bool rule_hit = false;
    if (it->isOverlapping(a)) // read overlaps a region
      for (auto jt : it->m_abstract_rules) { // loop rules in that region
	if (jt.isValid(a)) {
	  rule_hit = true;
	  break;
	}
	which_rule++;
      }

    // found a hit in a rule
    if (rule_hit)
      break;
    // didnt find hit, move it up one
    which_region++;
  }
  
  // isn't in a rule or it never satisfied one. Remove
  if (which_region >= m_regions.size())
    return ""; 

  string out = "rg" + to_string(++which_region) + "rl" + to_string(++which_rule);
  return out; 
  
}

// convert a region BED file into an interval tree map
void MiniRules::setIntervalTreeMap(string file) {
  
  m_region_file = file;
  GenomicRegionVector grv = GenomicRegion::regionFileToGRV(file, pad);
  m_grv = GenomicRegion::mergeOverlappingIntervals(grv); 
  sort(m_grv.begin(), m_grv.end());

  // set the width
  for (auto it : m_grv)
    m_width += it.width();
 
  size_t grv_size = m_grv.size();
  if (grv_size == 0) {
    cerr << "Warning: No regions dected in file: " << file << endl;
    return;
  }

  m_tree = GenomicRegion::createTreeMap(m_grv);
  return;
}

// constructor to make a MiniRulesCollection from a rules file.
// This will reduce each individual BED file and make the 
// GenomicIntervalTreeMap
MiniRulesCollection::MiniRulesCollection(string file) {

  // parse the rules file
  vector<string> region_files;
  ifstream iss_file(file.c_str());
  char delim = '%';
  if (iss_file) {
    string temp;
    file = "";
    while(getline(iss_file, temp)) {
      file += temp + "\n";
    }
    iss_file.close();
    delim = '\n';
  }
  istringstream iss_rules(file.c_str());
  
  // loop through the rules file and grab the rules
  string line;
  int level = 1;

  // define a default rule set
  vector<AbstractRule> all_rules;

  // default a default rule
  AbstractRule rule_all;
  
  while(getline(iss_rules, line, delim)) {

    //exclude comments and empty lines
    bool line_empty = line.find_first_not_of("\t\n ") == string::npos;
    bool line_comment = false;
    if (!line_empty)
      line_comment = line.at(0) == '#';
    
    if (!line_comment && !line_empty) {

      cout << line << endl;
      //////////////////////////////////
      // its a rule line, get the region
      //////////////////////////////////
      if (line.find("region@") != string::npos) {
	
	// check that the last one isnt empty. 
	// if it is, add the global to it
	if (m_regions.size() > 0)
	  if (m_regions.back()->m_abstract_rules.size() == 0)
	    m_regions.back()->m_abstract_rules.push_back(rule_all);

	// start a new MiniRule set
	MiniRules * mr = new MiniRules();
	
	// add the defaults
	//mr->m_abstract_rules = all_rules;

	// check if the mate aplies
	if (line.find(";mate") != string::npos) {
	  mr->m_applies_to_mate = true;
	}
	// check if we should pad 
	regex reg_pad(".*?;pad:(.*?)(;|$)");
	smatch pmatch;
	if (regex_search(line,pmatch,reg_pad))
	  try { mr->pad = stoi(pmatch[1].str()); } catch (...) { cerr << "Cant read pad value for line " << line << ", setting to 0" << endl; }
	  

	if (line.find("@WG") != string::npos) {
	  mr->m_whole_genome = true;
        } else {
	  regex file_reg("region@(.*?)(;|$)");
	  smatch match;
	  if (regex_search(line,match,file_reg))
	    mr->setIntervalTreeMap(match[1].str());
	  else {
	    cerr << "Could not parse line: " << line << " to grab region " << endl;
	    exit(EXIT_FAILURE);
	  }
	}
	mr->m_level = level++;
	m_regions.push_back(mr);
      }

      ////////////////////////////////////
      // its an rule
      ////////////////////////////////////
      else if (line.find("rule@") != string::npos) {
	AbstractRule ar = rule_all;
	ar.parseRuleLine(line);
	m_regions.back()->m_abstract_rules.push_back(ar);
      }
      ////////////////////////////////////
      // its a global rule
      ///////////////////////////////////
      else if (line.find("global@") != string::npos) {
	rule_all.parseRuleLine(line);
      }

    } //end comment check
  } // end \n parse

  // check that the last one isnt empty. 
  // if it is, add the global to it
  if (m_regions.size() > 0)
    if (m_regions.back()->m_abstract_rules.size() == 0)
      m_regions.back()->m_abstract_rules.push_back(rule_all);
  
  
  
}

// print the MiniRulesCollectoin
ostream& operator<<(ostream &out, const MiniRulesCollection &mr) {

  cout << "----------MiniRulesCollection-------------" << endl;
  for (auto it : mr.m_regions)
    out << (*it);
  cout << "------------------------------------------" << endl;
  return out;

}

// print a MiniRules information
ostream& operator<<(ostream &out, const MiniRules &mr) {
  
  string file_print = mr.m_whole_genome ? "WHOLE GENOME" : VarUtils::getFileName(mr.m_region_file);
  out << "--Region: " << file_print;;
  if (!mr.m_whole_genome) {
    out << " --Size: " << VarUtils::AddCommas<int>(mr.m_width); 
    out << " --Pad: " << mr.pad;
    out << " --Include Mate: " << (mr.m_applies_to_mate ? "ON" : "OFF") << endl;
  } else {
    out << endl;
  }
  for (auto it : mr.m_abstract_rules) 
    out << it << endl;
  
  return out;
}

// merge all of the intervals into one and send to a bed file
void MiniRulesCollection::sendToBed(string file) {

  ofstream out(file);
  if (!out) {
    cerr << "Cannot write BED file: " << file << endl;
    return;
  }
  out.close();

  GenomicRegionVector merged = sendToGrv();
  // send to BED file
  GenomicRegion::sendToBed(merged, file);
  return;
}

// parse a rule line looking for flag values
void FlagRule::parseRuleLine(string line) {

  istringstream iss(line);
  string val;
  while (getline(iss, val, ';')) {
    regex reg("!?(.*)");
    smatch match;
    if (regex_search(val, match, reg)) { // it matches a conditions
      auto ff = flags.find(match[1].str()); 
      if (ff != flags.end() && val.at(0) == '!') // it is a val in flags and is off
	ff->second.setOff();
      else if (ff != flags.end()) // is in a val in flags and is on
	ff->second.setOn();
    }
  }

}

// modify the rules based on the informaiton provided in the line
void AbstractRule::parseRuleLine(string line) {

  // get the name
  regex reg_name("(.*?)@.*");
  smatch nmatch;
  if (regex_search(line, nmatch, reg_name)) {
    name = nmatch[1].str();
  } else {
    cerr << "Name required for rules. e.g. myrule@" << endl;
    exit(EXIT_FAILURE);
  }

  // get everything but the name
  regex reg_noname(".*?@(.*)");
  smatch nnmatch;
  string noname;
  if (regex_search(line, nnmatch, reg_noname)) {
    noname = nnmatch[1].str();
  } else {
    cerr << "Rule is empty. Non-empty rule required. eg. myrule@!isize:[0,800]" << endl;
    exit(EXIT_FAILURE);
  }

  // check for every/none flags
  if (noname.find("all") != string::npos) 
    setEvery();
  if (noname.find("!all") != string::npos)
    setNone();

  // modify the ranges if need to
  isize.parseRuleLine(noname);
  mapq.parseRuleLine(noname);
  len.parseRuleLine(noname);
  clip.parseRuleLine(noname);
  phred.parseRuleLine(noname);
  nm.parseRuleLine(noname);

  // parse the line for flag rules
  fr.parseRuleLine(noname);
  
}

// parse for range
void Range::parseRuleLine(string line) {
  
  istringstream iss(line);
  string val;
  while (getline(iss, val, ';')) {
    
    string i_reg_str = "!" + pattern + ":?\\[(.*?),(.*?)\\]";
    string   reg_str = pattern + ":?\\[(.*?),(.*?)\\]";
    
    string n_reg_str = pattern + ":?!all";
    string a_reg_str = pattern + ":?all";
    
    regex ireg(i_reg_str);
    regex  reg(reg_str);
    regex nreg(n_reg_str);
    regex areg(a_reg_str);
    
    smatch match;
    //if (regex_search(val, match, nreg)) {
      //setNone();
    if (regex_search(val, match, areg)) {
      setEvery();
    } else if (regex_search(val, match, ireg)) {
      try {
	min = stoi(match[1].str());
	max = stoi(match[2].str());
	inverted = true;
	every = false; none = false;
	return;
      } catch (...) {
	cerr << "Caught error trying to parse inverted for " << pattern << " on line " << line << " match[1] " << match[1].str() << " match[2] " << match[2].str() << endl;     
	return;
      }
    } else if (regex_search(val, match, reg)) {
      try {
	min = stoi(match[1].str());
	max = stoi(match[2].str());
	inverted = false;
	every = false; none = false;
	return;
      } catch (...) {
	cerr << "Caught error trying to parse for " << pattern << " on line " << line << " match[1] " << match[1].str() << " match[2] " << match[2].str() << endl;     
	return;
      }
    }
    
  } // end getline
}

// main function for determining if a read is valid
bool AbstractRule::isValid(BamAlignment &a) {
  
  // check if its keep all or none
  if (isEvery())
    return true;

  // check if is discordant
  bool isize_pass = isize.isValid(abs(a.InsertSize));
  
  // check that read orientation is as expected
  if (!isize_pass) {
    bool FR_f = !a.IsReverseStrand() && (a.Position < a.MatePosition) && (a.RefID == a.MateRefID) &&  a.IsMateReverseStrand();
    bool FR_r =  a.IsReverseStrand() && (a.Position > a.MatePosition) && (a.RefID == a.MateRefID) && !a.IsMateReverseStrand();
    bool FR = FR_f || FR_r;
    isize_pass = isize_pass || !FR;
  }
  if (!isize_pass) {
    return false;
  }

  // check for valid mapping quality
  if (!mapq.isValid(a.MapQuality)) 
    return false;
  
  // check for valid flags
  if (!fr.isValid(a))
    return false;
  
  // check for valid NM
  uint32_t nm_val;
  if (!a.GetTag("NM",nm_val))
    nm_val = 0;
  int nm2 = nm_val;
  if (!nm.isValid(nm2))
    return false;

  // trim the read, then check length
  unsigned clipnum = VariantBamReader::getClipCount(a);
  string trimmed_bases = a.QueryBases;
  string trimmed_quals = a.Qualities;
  VariantBamReader::qualityTrimRead(phred.min, trimmed_bases, trimmed_quals); 
  int new_len = trimmed_bases.length();
  int new_clipnum = max(0, static_cast<int>(clipnum - (a.Length - new_len)));

  // check for valid length
  if (!len.isValid(new_len))
    return false;

  // check for valid clip
  if (!clip.isValid(new_clipnum))
    return false;

  return true;
}

bool FlagRule::isValid(BamAlignment &a) {
  
  if (isEvery())
    return true;
  
  if ( (flags["duplicate"].isOff() &&  a.IsDuplicate()) || (flags["duplicate"].isOn() && !a.IsDuplicate()) )
    return false;
  if ( (flags["supplementary"].isOff()      && !a.IsPrimaryAlignment()) || (flags["supplementary"].isOn() && a.IsPrimaryAlignment()) )
    return false;
  if ( (flags["qcfail"].isOff()    && a.IsFailedQC()) || (flags["qcfail"].isOn() && !a.IsFailedQC()) )
    return false;
  if ( (flags["fwd_strand"].isOff()    && !a.IsReverseStrand()) || (flags["fwd_strand"].isOn() && a.IsReverseStrand()) )
    return false;
  if ( (flags["rev_strand"].isOff()    &&  a.IsReverseStrand()) || (flags["rev_strand"].isOn() && !a.IsReverseStrand()) )
    return false;
  if ( (flags["mate_fwd_strand"].isOff()    && !a.IsMateReverseStrand()) || (flags["mate_fwd_strand"].isOn() && a.IsMateReverseStrand()) )
    return false;
  if ( (flags["mate_rev_strand"].isOff()    &&  a.IsMateReverseStrand()) || (flags["mate_rev_strand"].isOn() && !a.IsMateReverseStrand()) )
    return false;
  if ( (flags["mapped"].isOff()    &&  a.IsMapped()) || (flags["mapped"].isOn() && !a.IsMapped()) )
    return false;
  if ( (flags["mate_mapped"].isOff()    &&  a.IsMateMapped()) || (flags["mate_mapped"].isOn() && !a.IsMateMapped()) )
    return false;

  // check for hard clips
  if (!flags["hardclip"].isNA())  {// check that we want to chuck hard clip
    bool ishclipped = false;
    for (auto cig : a.CigarData)
      if (cig.Type == 'H') {
	ishclipped = true;
	break;
      }
    if ( (ishclipped && flags["hardclipped"].isOff()) || (!ishclipped && flags["hardclipped"].isOn()) )
      return false;
  }
  
  return true;
  
}

// define how to print
ostream& operator<<(ostream &out, const AbstractRule &ar) {

  out << "  Rule: " << ar.name << " -- ";;
  if (ar.isEvery()) {
    out << "  KEEPING ALL" << endl;
  } else if (ar.isNone()) {
    out << "  KEEPING NONE" << endl;
  } else {
    out << "isize:" << ar.isize << " -- " ;
    out << "mapq:" << ar.mapq << " -- " ;
    out << "len:" << ar.len << " -- ";
    out << "clip:" << ar.clip << " -- ";
    out << "phred:" << ar.phred << " -- ";
    out << "nm:" << ar.nm << " -- ";
    out << ar.fr;
  }
  return out;
}

// define how to print
ostream& operator<<(ostream &out, const FlagRule &fr) {

  if (fr.isEvery()) {
    out << "  Flag: ALL";
    return out;
  } 

  string keep = "  Flag ON: ";
  string remo = "  Flag OFF: ";
  string na = "  Flag NA: ";

  // get the strings
  for (auto it : fr.flags) {
    if (it.second.isNA())
      na += it.first + ",";
    else if (it.second.isOn())
      keep += it.first + ",";
    else if (it.second.isOff())
      remo += it.first + ",";
    else // shouldn't get here
      exit(1); 
  }

  out << keep << " -- " << remo << " -- " << na;
  
  return out;
}

// define how to print
ostream& operator<<(ostream &out, const Range &r) {
  if (r.inverted && r.min == -1 && r.max == -1)
    out << "all";
  else
    out << (r.inverted ? "NOT " : "") << "[" << r.min << "," << r.max << "]";
  return out;
}

// convert a MiniRulesCollection into a GRV
GenomicRegionVector MiniRulesCollection::sendToGrv() const {

  // make a composite
  GenomicRegionVector comp;
  for (auto it : m_regions)
    comp.insert(comp.begin(), it->m_grv.begin(), it->m_grv.end()); 
  
  // merge it down
  GenomicRegionVector merged = GenomicRegion::mergeOverlappingIntervals(comp);

  return merged;}
