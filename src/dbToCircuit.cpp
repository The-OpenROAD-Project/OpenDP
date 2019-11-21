#include "circuit.h"
#include <cfloat>

namespace opendp {

using std::max;
using std::min;
using std::pair;
using std::cout;
using std::cerr;
using std::vector;
using std::make_pair;
using std::to_string;
using std::string;
using std::fixed;
using std::numeric_limits;
using std::ifstream;
using std::ofstream;
using std::endl;

using odb::dbRowDir;
using odb::adsRect;
using odb::dbOrientType;
using odb::dbPlacementStatus;
using odb::dbOrientType;

// Row Sort Function
static bool SortByRowCoordinate(const row& lhs,
				const row& rhs);
// Row Generation Function
static vector<opendp::row> GetNewRow(const circuit* ckt);
static std::pair<double, double> 
GetOrientSize( double w, double h, dbOrientType orient );

void
circuit::db_to_circuit()
{
  // LEF
  DEFdist2Microns = db->getTech()->getDbUnitsPerMicron();
  for (auto db_lib : db->getLibs()) {
    make_sites(db_lib);
    make_macros(db_lib);
  }

  block = db->getChip()->getBlock();
  adsRect die_area;
  block->getDieArea(die_area);
  lx = die.xLL = die_area.xMin();
  by = die.yLL = die_area.yMin();
  rx = die.xUR = die_area.xMax();
  ty = die.yUR = die_area.yMax();

  make_rows();
  make_cells();

  // bizzare. -cherry
  lx = die.xLL = 0.0;
  by = die.yLL = 0.0;
  rx = die.xUR = core.xUR - core.xLL;
  ty = die.yUR = core.yUR - core.yLL;

  // sort ckt->rows
  sort(prevrows.begin(), prevrows.end(), SortByRowCoordinate);
  // change ckt->rows as CoreArea;
  rows = GetNewRow(this);

  cout << "CoreArea: " << endl;
  core.print();
  cout << "DieArea: " << endl;
  die.print();
}

void
circuit::make_sites(dbLib *db_lib)
{
  auto db_sites = db_lib->getSites();
  sites.reserve(db_sites.size());
  for (auto db_site : db_sites) {
    sites.push_back(site());
    struct site &site = sites.back();
    db_site_map[db_site] = &site;
    site.db_site = db_site;

    site.name = db_site->getConstName();
    site.width = dbuToMicrons(db_site->getWidth());
    site.height = dbuToMicrons(db_site->getHeight());
    site.type = db_site->getClass().getString();
    if (db_site->getSymmetryX())
      site.symmetries.push_back("X");
    if (db_site->getSymmetryY())
      site.symmetries.push_back("Y");
    if (db_site->getSymmetryR90())
      site.symmetries.push_back("R90");
  }
}

void
circuit::make_macros(dbLib *db_lib)
{
  auto db_masters = db_lib->getMasters();
  macros.reserve(db_masters.size());
  for (auto db_master : db_masters) {
    macros.push_back(macro());
    struct macro &macro = macros.back();
    db_master_map[db_master] = &macro;

    macro.db_master = db_master;
    macro.name = db_master->getConstName();
    macro.type = db_master->getType().getString();

    int x, y;
    db_master->getOrigin(x, y);
    macro.xOrig = dbuToMicrons(x);
    macro.yOrig = dbuToMicrons(y);

    macro.width = dbuToMicrons(db_master->getWidth());
    macro.height = dbuToMicrons(db_master->getHeight());
    struct site *site = db_site_map[db_master->getSite()];
    int site_idx = site - &sites[0];
    macro.sites.push_back(site_idx);

    make_macro_pins(db_master, macro);
    make_macro_obstructions(db_master, macro);
    macro_define_top_power(&macro);
  }
}

void
circuit::make_macro_pins(dbMaster *db_master,
			 struct macro &macro)
{
  for (auto db_mterm : db_master->getMTerms()) {
    for (auto db_mpin : db_mterm->getMPins()) {
      string pinName = db_mterm->getConstName();
      macro_pin &pin = macro.pins[pinName];

      pin.db_mpin = db_mpin;
      pin.direction = db_mterm->getIoType().getString();
      for (auto db_box : db_mpin->getGeometry()) {
	rect tmpRect;
	tmpRect.xLL = db_box->xMin();
	tmpRect.yLL = db_box->yMin();
	tmpRect.xUR = db_box->xMax();
	tmpRect.yUR = db_box->yMax();
	pin.port.push_back(tmpRect);
      }
      
    }
  }
}

void
circuit::make_macro_obstructions(dbMaster *db_master,
				 struct macro &macro)
{
  for (auto db_box : db_master->getObstructions()) {
    rect tmpRect;
    tmpRect.xLL = db_box->xMin();
    tmpRect.yLL = db_box->yMin();
    tmpRect.xUR = db_box->xMax();
    tmpRect.yUR = db_box->yMax();
    macro.obses.push_back(tmpRect);
  }
}

// - - - - - - - define multi row cell & define top power - - - - - - - - //
void circuit::macro_define_top_power(macro* myMacro) {

  bool isVddFound = false, isVssFound = false;
  string vdd_str, vss_str;

  auto pinPtr = myMacro->pins.find("vdd");
  if(pinPtr != myMacro->pins.end()) {
    vdd_str = "vdd";
    isVddFound = true;
  }
  else if( pinPtr != myMacro->pins.find("VDD") ) {
    vdd_str = "VDD";
    isVddFound = true;
  }

  pinPtr = myMacro->pins.find("vss");
  if( pinPtr != myMacro->pins.end()) {
    vss_str = "vss";
    isVssFound = true;
  }
  else if( pinPtr != myMacro->pins.find("VSS") ) {
    vss_str = "VSS";
    isVssFound = true;
  }


  if( isVddFound || isVssFound ) {
    double max_vdd = 0;
    double max_vss = 0;

    macro_pin* pin_vdd = NULL;
    if( isVddFound ) {
      pin_vdd = &myMacro->pins.at(vdd_str);
      for(int i = 0; i < pin_vdd->port.size(); i++) {
        if(pin_vdd->port[i].yUR > max_vdd) {
          max_vdd = pin_vdd->port[i].yUR;
        } 
      }
    }
   
    macro_pin* pin_vss = NULL;
    if( isVssFound ) {
      pin_vss = &myMacro->pins.at(vss_str);
      for(int j = 0; j < pin_vss->port.size(); j++) {
        if(pin_vss->port[j].yUR > max_vss) {
          max_vss = pin_vss->port[j].yUR;
        }
      }
    }

    if(max_vdd > max_vss)
      myMacro->top_power = VDD;
    else
      myMacro->top_power = VSS;

    if(pin_vdd && pin_vss) {
      if (pin_vdd->port.size() + pin_vss->port.size() > 2) {
	// This fails when a polygon is converted into muliple rects.
	//        myMacro->isMulti = true;
      } 
      else if(pin_vdd->port.size() + pin_vss->port.size() < 2) {
        cerr << "macro:: power num error, vdd + vss => "
           << (pin_vdd->port.size() + pin_vss->port.size()) << endl;
        exit(1);
      }
    }
  }
}

void
circuit::make_rows()
{
  auto db_rows = block->getRows();
  rows.reserve(db_rows.size());
  for (auto db_row : db_rows) {
    prevrows.push_back(row());
    struct row &row = prevrows.back();

    row.db_row = db_row;
    const char *row_name = db_row->getConstName();
    row.name = row_name;
    site *site = db_site_map[db_row->getSite()];
    int site_idx = site - &sites[0];
    row.site = site_idx;
    int x, y;
    db_row->getOrigin(x, y);
    row.origX = x;
    row.origY = y;
    row.siteorient = db_row->getOrient().getString();
    row.numSites = db_row->getSiteCount();
    switch (db_row->getDirection()) {
    case dbRowDir::HORIZONTAL:
      row.stepX = db_row->getSpacing();
      row.stepY = 0;
      break;
    case dbRowDir::VERTICAL:
      row.stepX = 0;
      row.stepY = db_row->getSpacing();
      break;
    }

    // initialize rowHeight variable (double)
    if( fabs(rowHeight - 0.0f) <= DBL_EPSILON ) {
      rowHeight = sites[ row.site ].height * DEFdist2Microns;
    }

    // initialize wsite variable (int)
    if( wsite == 0 ) {
      wsite = int(sites[row.site].width * DEFdist2Microns + 0.5f);
    }
  
    core.xLL = min(1.0*row.origX, core.xLL);
    core.yLL = min(1.0*row.origY, core.yLL);
    core.xUR = max(1.0*row.origX + row.numSites * wsite, core.xUR);
    core.yUR = max(1.0*row.origY + rowHeight, core.yUR);
  }
}

// Y first and X second
// First row should be in the first index to get orient
static bool SortByRowCoordinate (const row& lhs,
				 const row& rhs) {
  if( lhs.origY < rhs.origY ) {
    return true;
  }
  if( lhs.origY > rhs.origY ) {
    return false;
  }

  return ( lhs.origX < rhs.origX );
}

// Generate New Row Based on CoreArea
static vector<opendp::row> GetNewRow(const circuit* ckt) {
  // Return Row Vectors
  vector<opendp::row> retRow;

  // calculation X and Y from CoreArea
  int rowCntX = IntConvert((ckt->core.xUR - ckt->core.xLL)/ckt->wsite);
  int rowCntY = IntConvert((ckt->core.yUR - ckt->core.yLL)/ckt->rowHeight);

  unsigned siteIdx = ckt->prevrows[0].site;
  string curOrient = ckt->prevrows[0].siteorient;

  for(int i=0; i<rowCntY; i++) {
    opendp::row myRow;
    myRow.site = siteIdx;
    myRow.origX = IntConvert(ckt->core.xLL);
    myRow.origY = IntConvert(ckt->core.yLL + i * ckt->rowHeight);

    myRow.stepX = ckt->wsite;
    myRow.stepY = 0;

    myRow.numSites = rowCntX;
    myRow.siteorient = curOrient;
    retRow.push_back(myRow);

    // curOrient is flipping. e.g. N -> FS -> N -> FS -> ...
    curOrient = (curOrient == "N")? "FS" : "N";
  }
  return retRow;
}


void
circuit::make_cells()
{
  auto db_insts = block->getInsts();
  cells.reserve(db_insts.size());
  for (auto db_inst : db_insts) {
    cells.push_back(cell());
    struct cell &cell = cells.back();
    cell.db_inst = db_inst;
    db_inst_map[db_inst] = &cell;

    cell.name = db_inst->getConstName();
    dbMaster *master = db_inst->getMaster();
    auto miter = db_master_map.find(master);
    if (miter != db_master_map.end()) {
      macro *macro = miter->second;
      int macro_idx = macro - &macros[0];
      cell.type = macro_idx;
   
      dbOrientType orient = db_inst->getOrient();
      pair<double, double> orientSize 
	= GetOrientSize( macro->width, macro->height, orient);

      cell.width = orientSize.first * static_cast<double> (DEFdist2Microns);
      cell.height = orientSize.second * static_cast<double> (DEFdist2Microns);

      cell.isFixed = (db_inst->getPlacementStatus() == dbPlacementStatus::FIRM);
  
      // Shift by core.xLL and core.yLL
      int x, y;
      db_inst->getLocation(x, y);
      cell.init_x_coord = std::max(0.0, x - core.xLL);
      cell.init_y_coord = std::max(0.0, y - core.yLL);

      // fixed cells
      if( cell.isFixed ) {
	// Shift by core.xLL and core.yLL
	cell.x_coord = x - core.xLL;
	cell.y_coord = y - core.yLL;
	cell.isPlaced = true;
      }
      cell.cellorient = orient.getString();
    }
  }
}

// orient coordinate shift 
static std::pair<double, double> 
GetOrientSize( double w, double h, dbOrientType orient ) {
  switch(orient) {
  case dbOrientType::R90:
  case dbOrientType::MXR90:
  case dbOrientType::R270:
  case dbOrientType::MYR90:
    return std::make_pair(h, w);
  case dbOrientType::R0:
  case dbOrientType::R180:
  case dbOrientType::MY:
  case dbOrientType::MX:
    return std::make_pair(w, h); 
  }
}

////////////////////////////////////////////////////////////////
#if 0

opendp::group* CircuitParser::topGroup_ = 0;

int circuit::ReadDef() {

  defrSetComponentStartCbk(cp.DefStartCbk);
  defrSetNetStartCbk(cp.DefStartCbk);
  defrSetSNetStartCbk(cp.DefStartCbk);
  defrSetStartPinsCbk(cp.DefStartCbk);

  defrSetSNetEndCbk(cp.DefEndCbk);

  // Components
  defrSetComponentCbk(cp.DefComponentCbk);
  
  // pins
  defrSetPinCbk((defrPinCbkFnType)cp.DefPinCbk);

  // Nets
  defrSetSNetCbk(cp.DefSNetCbk);
  defrSetAddPathToNet();

  // Regions
  defrSetRegionCbk((defrRegionCbkFnType)cp.DefRegionCbk);

  // Groups
  defrSetGroupNameCbk((defrStringCbkFnType)cp.DefGroupNameCbk);
  defrSetGroupMemberCbk((defrStringCbkFnType)cp.DefGroupMemberCbk);

  // End Design
  defrSetDesignEndCbk(cp.DefEndCbk);
  return 0;
}

// orient coordinate shift 
inline static std::pair<double, double> 
GetOrientPoint( double x, double y, double w, double h, int orient ) {
  switch(orient) {
    case 0: // North
      return std::make_pair(x, y); 
    case 1: // West
      return std::make_pair(h-y, x);
    case 2: // South
      return std::make_pair(w-x, h-y); // x-flip, y-flip
    case 3: // East
      return std::make_pair(y, w-x);
    case 4: // Flipped North
      return std::make_pair(w-x, y); // x-flip
    case 5: // Flipped West
      return std::make_pair(y, x); // x-flip from West
    case 6: // Flipped South
      return std::make_pair(x, h-y); // y-flip
    case 7: // Flipped East
      return std::make_pair(h-y, w-x); // y-flip from West
  }
}

// Get Lower-left coordinates from rectangle's definition
inline static std::pair<double, double> 
GetOrientLowerLeftPoint( double lx, double ly, double ux, double uy,
   double w, double h, int orient ) {
  switch(orient) {
    case 0: // North
      return std::make_pair(lx, ly); // verified
    case 1: // West
      return GetOrientPoint(lx, uy, w, h, orient);
    case 2: // South
      return GetOrientPoint(ux, uy, w, h, orient);
    case 3: // East
      return GetOrientPoint(ux, ly, w, h, orient);
    case 4: // Flipped North
      return GetOrientPoint(ux, ly, w, h, orient); // x-flip
    case 5: // Flipped West
      return GetOrientPoint(lx, ly, w, h, orient); // x-flip from west
    case 6: // Flipped South
      return GetOrientPoint(lx, uy, w, h, orient); // y-flip
    case 7: // Flipped East
      return GetOrientPoint(ux, uy, w, h, orient); // y-flip from west
  }
}


//////////////////////////////////////////////////
// DEF Parsing Cbk Functions
//

// DEF's Start callback to reserve memories
int CircuitParser::DefStartCbk(
    defrCallbackType_e c,
    int num,
    defiUserData ud) {
  circuit* ckt = (circuit*) ud;
  switch(c) {
    case defrComponentStartCbkType:
      ckt->cells.reserve(num);
      break;
    case defrStartPinsCbkType:
      ckt->pins.reserve(num);
      break;
    case defrSNetStartCbkType:
      ckt->minVddCoordiY = DBL_MAX;
      break;
  }
  return 0; 
}

// DEF's End callback
int CircuitParser::DefEndCbk(
    defrCallbackType_e c,
    void*,
    defiUserData ud) {

  circuit* ckt = (circuit*) ud;
  switch(c) {
    case defrSNetEndCbkType:
      // fill initial_power information
      if( (int(ckt->minVddCoordiY+0.5f) / int(ckt->rowHeight+0.5f)) % 2 == 0 ) {
        ckt->initial_power = VDD; 
      }
      else {
        ckt->initial_power = VSS;
      }
      break;
  
  }
  return 0;
}

int CircuitParser::DefPinCbk(
    defrCallbackType_e c, 
    defiPin* pi,
    defiUserData ud) {
  
  circuit* ckt = (circuit*) ud;
  pin* myPin = ckt->locateOrCreatePin( pi->pinName() );
  if( strcmp(pi->direction(), "INPUT") == 0 ) {
    myPin -> type = PI_PIN;
  }
  else if( strcmp(pi->direction(), "OUTPUT") == 0 ) {
    myPin -> type = PO_PIN;
  }

  myPin->isFixed = pi->isFixed();

  // Shift by core.xLL and core.yLL
  myPin->x_coord = pi->placementX() - ckt->core.xLL;
  myPin->y_coord = pi->placementY() - ckt->core.yLL;

  return 0;
}

// DEF's NET
int CircuitParser::DefNetCbk(
    defrCallbackType_e c,
    defiNet* dnet, 
    defiUserData ud) {
  circuit* ckt = (circuit*) ud;
  net* myNet = NULL;

  myNet = ckt->locateOrCreateNet( dnet->name() );
  unsigned myNetId = ckt->net2id.find(myNet->name)->second;

  // subNet iterations
  for(int i=0; i<dnet->numConnections(); i++) {
    // Extract pin informations
    string pinName = (strcmp(dnet->instance(i), "PIN") == 0)? 
      dnet->pin(i) : 
      string(dnet->instance(i)) + ":" + string(dnet->pin(i));

    pin* myPin = ckt->locateOrCreatePin( pinName );
    myPin->net = myNetId;

    // source setting
    if( i == 0 ) {
      myNet->source = myPin->id; 
    }

    // if it is cell inst's pin (e.g. not equal to "PIN")
    // Fill owner's info
    if( strcmp(dnet->instance(i), "PIN") != 0 ) {
      myPin->owner = ckt->cell2id[ dnet->instance(i) ];
      myPin->type = NONPIO_PIN;

//      cout << "owner's name: " << dnet->instance(i) << endl;
//      cout << "owner: " << myPin->owner << endl;
//      cout << "type: " << ckt->cells[myPin->owner].type << endl; 
      macro* theMacro = &ckt->macros[ ckt->cells[myPin->owner].type ];
//      cout << "theMacro: " << theMacro << endl;
//      cout << "theMacro pin size: " << theMacro->pins.size() << endl;
//      cout << "dnet->pin: " << dnet->pin(i) << endl;
      macro_pin* myMacroPin = &theMacro->pins[ dnet->pin(i) ];
//      cout << "myMacroPin: " << myMacroPin << endl;
//      cout << "port: " << myMacroPin->port.size() << endl;

      if( myMacroPin-> port.size() == 0 ) {
        cout << "ERROR: in Net " << dnet->name() 
          << " has a module:pin definition as " << pinName 
          << " but there is no PORT/PIN definition in LEF MACRO: " 
          << theMacro->name << endl;
        exit(1);
      }
      myPin->x_offset = 
        myMacroPin->port[0].xLL / 2 + myMacroPin->port[0].xUR / 2;
      myPin->y_offset = 
        myMacroPin->port[0].yLL / 2 + myMacroPin->port[0].yUR / 2;
//      cout << "calculated x_offset: " << myPin->x_offset << endl;
//      cout << "calculated y_offset: " << myPin->y_offset << endl;
    }

    if( i != 0 ){
      myNet->sinks.push_back(myPin->id);
    }
  }
  return 0;
}

// DEF's SPECIALNETS
// Extract VDD/VSS row informations for mixed-height legalization
int CircuitParser::DefSNetCbk(
    defrCallbackType_e c,
    defiNet* swire, 
    defiUserData ud) {
  
  circuit* ckt = (circuit*) ud;

  // Check the VDD values
  if( strcmp("vdd", swire->name()) == 0 ||
      strcmp("VDD", swire->name()) ) {
    if( swire->numWires() ) {
      for(int i=0; i<swire->numWires(); i++) {
        defiWire* wire = swire->wire(i);

        for(int j=0; j<wire->numPaths(); j++) {
          defiPath* p = wire->path(j);
          p->initTraverse();
          int path = 0;
          int x1 = 0, y1 = 0, x3 = 0, y3 = 0;

          bool isMeetFirstLayer = false;
          bool isMeetFirstPoint = false;
          bool isViaRelated = false;

          int pathCnt = 0;
          while((path = (int)p->next()) != DEFIPATH_DONE ) {
            pathCnt++;
            switch(path) {
              case DEFIPATH_LAYER:
                // HARD CODE for extracting metal1. 
                // Need to be fixed later
                if( strcmp( p->getLayer(), "metal1") == 0) {
                  isMeetFirstLayer = true;
                  isMeetFirstPoint = false;
                }
                break;
              case DEFIPATH_POINT:
                if( isMeetFirstLayer ) {
                  if( !isMeetFirstPoint ) {
                    p->getPoint(&x1, &y1);
                    isMeetFirstPoint = true;
                  }
                  else {
                    p->getPoint(&x3, &y3);
                  }
                }
                break;
              case DEFIPATH_VIA:
                isViaRelated = true;
                break;

              default:
                break; 
            }
          }

          if( isMeetFirstLayer && !isViaRelated ) {
            ckt->minVddCoordiY = (ckt->minVddCoordiY < y1)? ckt->minVddCoordiY : y1;
//            cout << x1 << " " << y1 << " " << x3 << " " << y3 << endl;
          }
        }
      } 

    } 
  }
//  cout << "Final VDD min coordi: " << ckt->minVddCoordiY << endl;
  return 0;
}

// Fence region handling
// DEF's REGIONS -> call groups
int CircuitParser::DefRegionCbk(
    defrCallbackType_e c,
    defiRegion* re, 
    defiUserData ud) {

  circuit* ckt = (circuit*) ud;
  group* curGroup = ckt->locateOrCreateGroup(re->name());

  // initialize for BB
  curGroup->boundary.xLL = DBL_MAX;
  curGroup->boundary.yLL = DBL_MAX;
  curGroup->boundary.xUR = DBL_MIN;
  curGroup->boundary.yUR = DBL_MIN;

  for(int i = 0; i < re->numRectangles(); i++) {
    opendp::rect tmpRect;
    tmpRect.xLL = re->xl(i);
    tmpRect.yLL = re->yl(i);
    tmpRect.xUR = re->xh(i);
    tmpRect.yUR = re->yh(i); 
    
    // Extract BB
    curGroup->boundary.xLL = min(curGroup->boundary.xLL, tmpRect.xLL);
    curGroup->boundary.yLL = min(curGroup->boundary.yLL, tmpRect.yLL);
    curGroup->boundary.xUR = max(curGroup->boundary.xUR, tmpRect.xUR);
    curGroup->boundary.yUR = max(curGroup->boundary.yUR, tmpRect.yUR);
    
    // push rect info
    curGroup->regions.push_back( tmpRect );
  }
  curGroup->type = re->type();
  return 0;
}

// DEF's GROUPS -> call groups
int CircuitParser::DefGroupNameCbk(
    defrCallbackType_e c, 
    const char* name,
    defiUserData ud) {
  circuit* ckt = (circuit*) ud;
  topGroup_ = ckt->locateOrCreateGroup(name);
  return 0;
}

int CircuitParser::DefGroupMemberCbk(
    defrCallbackType_e c, 
    const char* name,
    defiUserData ud) {
  circuit* ckt = (circuit*) ud;

  topGroup_->tag = name;
  for(auto& curCell : ckt->cells) {
    // HARD CODE
    // Suppose tag is always SOMETH/*
    // need to port regexp lib later
    if(strncmp(topGroup_->tag.c_str(), curCell.name.c_str(),
          topGroup_->tag.size() - 1) == 0) {
      topGroup_->siblings.push_back(&curCell);
      curCell.group = topGroup_->name;
      curCell.inGroup = true;
    }
  } 

  return 0;
}

// Y first and X second
// First row should be in the first index to get orient
bool SortByRowCoordinate(
    const opendp::row& lhs,
    const opendp::row& rhs) {
  if( lhs.origY < rhs.origY ) {
    return true;
  }
  if( lhs.origY > rhs.origY ) {
    return false;
  }

  return ( lhs.origX < rhs.origX );
}

// Generate New Row Based on CoreArea
vector<opendp::row> GetNewRow(const circuit* ckt) {
  // Return Row Vectors
  vector<opendp::row> retRow;

  // calculation X and Y from CoreArea
  int rowCntX = IntConvert((ckt->core.xUR - ckt->core.xLL)/ckt->wsite);
  int rowCntY = IntConvert((ckt->core.yUR - ckt->core.yLL)/ckt->rowHeight);

  unsigned siteIdx = ckt->prevrows[0].site;
  string curOrient = ckt->prevrows[0].siteorient;

  for(int i=0; i<rowCntY; i++) {
    opendp::row myRow;
    myRow.site = siteIdx;
    myRow.origX = IntConvert(ckt->core.xLL);
    myRow.origY = IntConvert(ckt->core.yLL + i * ckt->rowHeight);

    myRow.stepX = ckt->wsite;
    myRow.stepY = 0;

    myRow.numSites = rowCntX;
    myRow.siteorient = curOrient;
    retRow.push_back(myRow);

    // curOrient is flipping. e.g. N -> FS -> N -> FS -> ...
    curOrient = (curOrient == "N")? "FS" : "N";
  }
  return retRow;
}
#endif

}
