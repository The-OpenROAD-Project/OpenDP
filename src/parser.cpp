/////////////////////////////////////////////////////////////////////////////
// Authors: SangGi Do(sanggido@unist.ac.kr), Mingyu Woo(mwoo@eng.ucsd.edu)
//          (respective Ph.D. advisors: Seokhyeong Kang, Andrew B. Kahng)
//
//          Original parsing structure was made by Myung-Chul Kim (IBM).
//
// BSD 3-Clause License
//
// Copyright (c) 2018, SangGi Do and Mingyu Woo
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "circuit.h"
#include <cmath>
//#include <iomanip>

#define _DEBUG

using opendp::circuit;
using opendp::cell;
using opendp::row;
using opendp::pixel;
using opendp::rect;

using std::max;
using std::min;
using std::pair;
using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::make_pair;
using std::to_string;
using std::string;
using std::fixed;
using std::setprecision;
using std::numeric_limits;

const char* FFClkPortName = "ck";

inline bool operator<(const row& a, const row& b) {
  return (a.origY < b.origY) || (a.origY == b.origY && a.origX < b.origX);
}

void circuit::InitOpendpAfterParse() {

  power_mapping();
  // summary of benchmark
  calc_design_area_stats();

  // dummy cell generation
  dummy_cell.name = "FIXED_DUMMY";
  dummy_cell.isFixed = true;
  dummy_cell.isPlaced = true;

  // calc row / site offset
  int row_offset = rows[0].origY;
  int site_offset = rows[0].origX;

  // construct pixel grid
  int row_num = std::round(ty / rowHeight);
  int col = std::round(rx / wsite);
  grid = new pixel*[row_num];
  for(int i = 0; i < row_num; i++) {
    grid[i] = new pixel[col];
  }

  for(int i = 0; i < row_num; i++) {
    for(int j = 0; j < col; j++) {
      grid[i][j].name = "pixel_" + to_string(i) + "_" + to_string(j);
      grid[i][j].y_pos = i;
      grid[i][j].x_pos = j;
      grid[i][j].linked_cell = NULL;
      grid[i][j].isValid = false;
    }
  }

  // Fragmented Row Handling
  for(auto& curFragRow : prevrows) {
    int x_start = IntConvert((1.0*curFragRow.origX - core.xLL) / wsite);
    int y_start = IntConvert((1.0*curFragRow.origY - core.yLL) / rowHeight);
    
    int x_end = x_start + curFragRow.numSites;
    int y_end = y_start + 1;

//    cout << "x_start: " << x_start << endl;
//    cout << "y_start: " << y_start << endl;
//    cout << "x_end: " << x_end << endl;
//    cout << "y_end: " << y_end << endl;
    for(int i=x_start; i<x_end; i++) {
      for(int j=y_start; j<y_end; j++) {
        grid[j][i].isValid = true;
      }
    }
  }

  // fixed cell marking
  fixed_cell_assign();
  // group id mapping & x_axis dummycell insertion
  group_pixel_assign_2();
  // y axis dummycell insertion
  group_pixel_assign();

  init_large_cell_stor();
}

void circuit::calc_design_area_stats() {
  num_fixed_nodes = 0;

  // total_mArea : total movable cell area that need to be placed
  // total_fArea : total fixed cell area.
  // designArea : total available place-able area.
  //
  total_mArea = total_fArea = designArea = 0.0;
  for(vector< cell >::iterator theCell = cells.begin(); theCell != cells.end();
      ++theCell) {
    if(theCell->isFixed) {
      total_fArea += theCell->width * theCell->height;
      num_fixed_nodes++;
    }
    else
      total_mArea += theCell->width * theCell->height;
  }
  for(vector< row >::iterator theRow = rows.begin(); theRow != rows.end();
      ++theRow)
    designArea += theRow->stepX * theRow->numSites *
                  sites[theRow->site].height *
                  static_cast< double >(DEFdist2Microns);

  unsigned multi_num = 0;
  for(int i = 0; i < cells.size(); i++) {
    cell* theCell = &cells[i];
    macro* theMacro = &macros[theCell->type];
    if(theMacro->isMulti == true) {
      multi_num++;
    }
  }

  for(int i = 0; i < cells.size(); i++) {
    cell* theCell = &cells[i];
    macro* theMacro = &macros[theCell->type];
    if(theCell->isFixed == false && 
        theMacro->isMulti == true && 
        theMacro->type == "CORE") {
      if(max_cell_height <
         static_cast< int >(theMacro->height * DEFdist2Microns / rowHeight +
                            0.5))
        max_cell_height = static_cast< int >(
            theMacro->height * DEFdist2Microns / rowHeight + 0.5);
    }
  }

  design_util = total_mArea / (designArea - total_fArea);

  cout << "-------------------- DESIGN ANALYSIS ------------------------------"
       << endl;
  cout << "  total cells              : " << cells.size() << endl;
  cout << "  multi cells              : " << multi_num << endl;
  cout << "  fixed cells              : " << num_fixed_nodes << endl;
  cout << "  total nets               : " << block->getNets().size() << endl;

  cout << "  design area              : " << designArea << endl;
  cout << "  total f_area             : " << total_fArea << endl;
  cout << "  total m_area             : " << total_mArea << endl;
  cout << "  design util              : " << design_util * 100.00 << endl;
  cout << "  num rows                 : " << rows.size() << endl;
  cout << "  row height               : " << rowHeight << endl;
  if(max_cell_height > 1)
    cout << "  max multi_cell height    : " << max_cell_height << endl;
  if(max_disp_const > 0)
    cout << "  max disp const           : " << max_disp_const << endl;
  if(groups.size() > 0)
    cout << "  group num                : " << groups.size() << endl;
  cout << "-------------------------------------------------------------------"
       << endl;

  // 
  // design_utilization error handling.
  //
  if( design_util >= 1.001 ) {
    cout << "ERROR:  Utilization exceeds 100%. (" 
      << fixed << setprecision(2) << design_util * 100.00  << "%)! ";
    cout << "        Please double check your input files!" << endl;
    exit(1); 
  }
}

bool circuit::read_constraints(const string& input) {
  //    cout << " .constraints file : " << input << endl;
  ifstream dot_constraints(input.c_str());
  if(!dot_constraints.good()) {
    cerr << "read_constraints:: cannot open '" << input << "' for reading"
         << endl;
    return true;
  }

  string context;

  while(!dot_constraints.eof()) {
    dot_constraints >> context;
    if(dot_constraints.eof()) break;
    if(strncmp(context.c_str(), "maximum_utilization", 19) == 0) {
      string temp = context.substr(0, context.find_last_of("%"));
      string max_util = temp.substr(temp.find_last_of("=") + 1);
      max_utilization = atof(max_util.c_str());
    }
    else if(strncmp(context.c_str(), "maximum_movement", 16) == 0) {
      string temp = context.substr(0, context.find_last_of("rows"));
      string max_move = temp.substr(temp.find_last_of("=") + 1);
      displacement = atoi(max_move.c_str()) * 20;
      max_disp_const = atoi(max_move.c_str());
    }
    else {
      cerr << "read_constraints:: unsupported keyword " << endl;
      return true;
    }
  }

  if(max_disp_const == 0.0) max_disp_const = rows.size();

  dot_constraints.close();
  return false;
}

void circuit::init_large_cell_stor() {
  large_cell_stor.reserve(cells.size());
  for(auto& curCell : cells) {
    large_cell_stor.push_back(
        make_pair(curCell.width * curCell.height, &curCell));
  }

  sort(large_cell_stor.begin(), large_cell_stor.end(),
       [](const pair< float, cell* >& lhs, const pair< float, cell* >& rhs) {
         return (lhs.first > rhs.first);
       });
  return;
}
