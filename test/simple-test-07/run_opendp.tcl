set exp_folder ./simple-test-07
set design ten_cells 

set TIME_start [clock clicks -milliseconds]

opendp_external odp
odp import_lef nangate45-bench/tech/NangateOpenCellLibrary.lef
odp import_def ${exp_folder}/${design}.def

odp init_opendp
odp legalize_place

set TIME_taken [expr [clock clicks -milliseconds] - $TIME_start]

set fp [open ${exp_folder}/${design}.rpt w]
set legality [odp check_legality]

puts $fp "Legality          : $legality"
puts $fp "Runtime           : $TIME_taken"
puts $fp "Sum displacement  : [odp get_sum_displacement]"
puts $fp "Avg displacement  : [odp get_average_displacement]"
puts $fp "Max displacement  : [odp get_max_displacement]"
puts $fp "Original HPWL     : [odp get_original_hpwl]"
puts $fp "Legalized HPWL    : [odp get_legalized_hpwl]"

close $fp

exit
