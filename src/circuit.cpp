#include "circuit.h"
#include <cfloat>

using std::cout;
using std::endl;
using std::string;

namespace opendp {

using odb::dbPlacementStatus;

rect::rect()
  : xLL(std::numeric_limits< double >::max()),
    yLL(std::numeric_limits< double >::max()),
    xUR(std::numeric_limits< double >::min()),
    yUR(std::numeric_limits< double >::min()) {};

  
void rect::print() { 
  printf("%f : %f - %f : %f\n", xLL, yLL, xUR, yUR);
  fflush(stdout); 
}

macro::macro()
  : isMulti(false),
    edgetypeLeft(0),
    edgetypeRight(0),
    top_power(power::undefined) {
#ifdef USE_GOOGLE_HASH
  pins.set_empty_key(INITSTR);
#endif
}

void macro::print() {
  cout << "|=== BEGIN MACRO ===|" << endl;
  cout << "name:                " << db_master->getConstName() << endl;
  cout << "|=== BEGIN MACRO ===|" << endl;
}

cell::cell()
      : id(UINT_MAX),
	cell_macro(nullptr),
        x_coord(0),
        y_coord(0),
        init_x_coord(0),
        init_y_coord(0),
        x_pos(INT_MAX),
        y_pos(INT_MAX),
        width(0.0),
        height(0.0),
        isPlaced(false),
        hold(false),
        region(UINT_MAX),
        cell_group(nullptr),
        dense_factor(0.0),
        dense_factor_count(0),
        binId(UINT_MAX),
        disp(0.0) {
#ifdef USE_GOOGLE_HASH
    ports.set_empty_key(INITSTR);
#endif
}

bool
circuit::isFixed(cell *cell1)
{
  return cell1 == &dummy_cell
    || cell1->db_inst->getPlacementStatus() == dbPlacementStatus::FIRM 
    || cell1->db_inst->getPlacementStatus() == dbPlacementStatus::LOCKED
    || cell1->db_inst->getPlacementStatus() == dbPlacementStatus::COVER;
}

pixel::pixel()
  : name(""),
    util(0.0),
    x_pos(0.0),
    y_pos(0.0),
    pixel_group(nullptr),
    linked_cell(NULL),
    isValid(true) {};

row::row()
      : name(""),
        site(nullptr),
	origX(0),
        origY(0),
        stepX(0),
        stepY(0),
        numSites(0),
        siteorient(dbOrientType::R0) {};

group::group() : name(""), type(""), tag(""), util(0.0) {};

void group::print(std::string gName) {
  std::cout << gName << " name : " << name << " type : " << type
       << " tag : " << tag << " end line " << std::endl;
  for(int i = 0; i < regions.size(); i++) {
    regions[i].print();
  }
};

sub_region::sub_region()
  : x_pos(0), y_pos(0), width(0), height(0) {
  siblings.reserve(8192);
}

circuit::circuit() 
  : GROUP_IGNORE(false),
    num_fixed_nodes(0),
    num_cpu(1),
    DEFdist2Microns(0),
    sum_displacement(0.0),
    max_displacement(0.0),
    avg_displacement(0.0),
    minVddCoordiY(DBL_MAX),
    initial_power(power::undefined),
    displacement(400.0),
    max_disp_const(0.0),
    max_utilization(100.0),
    wsite(0),
    max_cell_height(1),
    rowHeight(0.0f)
{
  rows.reserve(4096);
  sub_regions.reserve(100);
}

void 
circuit::clear()
{
  using std::vector;
  // clear the pre-built structures.
  if( macros.size() > 0 ) {
    vector<macro> ().swap( macros );
  }
  if( rows.size() > 0 ) {
    vector<row> ().swap( rows );
  } 
  if( prevrows.size() > 0 ) {
    vector<row> ().swap( prevrows );
  }
  if( cells.size() > 0 ) {
    vector<cell> ().swap(cells);
  } 
}

int IntConvert(double fp) {
  return (int)std::round(fp);
}

void
circuit::update_db_inst_locations()
{
  for(int i = 0; i < cells.size(); i++) {
    struct cell* cell = &cells[i];
    int x = IntConvert(cell->x_coord + core.xLL);
    int y = IntConvert(cell->y_coord + core.yLL);
    dbInst *db_inst = cell->db_inst;
    db_inst->setLocation(x, y);
    // Orientation is already set.
  }
}

}
