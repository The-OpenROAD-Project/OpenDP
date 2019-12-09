source helpers.tcl
read_lef nangate45-bench/tech/NangateOpenCellLibrary.lef
read_def simple-test-03.def
legalize_placement
set def_file [make_result_file simple-test-03-leg.def]
write_def $def_file
report_file $def_file
