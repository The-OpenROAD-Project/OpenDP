# OpenDP
*OpenDP: Open Source Detailed Placement Engine*

If you want to use this as part of the OpenROAD project you should build it and use it from inside the integrated [openroad app](https://github.com/The-OpenROAD-Project/OpenROAD). The standalone version is available as legacy code.

| <img src="/doc/image/OpenDP_sample.jpg" width=900px> | 
|:--:| 
| *Visualized detailed placement result using aes_cipher_top design with innovus initial placement; OpenDP_sample.jpg* |
### Required
* GCC compiler and libstdc++ static library >= 4.8.5
* Recommended OS: Centos6 or Centos7 or Ubuntu 16.04

### How To Compile
    $ git clone --recursive https://github.com/The-OpenROAD-Project/OpenDP.git
    $ cd OpenDP
    $ mkdir build
    $ cd build 
    $ cmake ..             // you may specify -DCMAKE_INSTALL_PREFIX to avoid installing in /usr/local/*
    $ make
    $ sudo make install    // or make install if you specified -DCMAKE_INSTALL_PREFIX

### How To Execute using Tcl Interpreter. 
    // Check doc/TclCommands.md in detail
    $ cd test/
    $ opendp < run_nangate45_gcd.tcl


### Verified/supported Technologies
* TSMC 65 (GP/LP)
* ST FDSOI 28
* TSMC 16 (9T)

### Manual
* [doc/TclCommands.md](doc/TclCommands.md)

### License
* BSD-3-clause License [[Link]](LICENSE)

### Authors
- SangGi Do and Mingyu Woo (respective Ph. D. advisors: Seokhyeong Kang, Andrew B. Kahng).
- parser_helper.cpp is based on Dr. Myung-Chul Kim(IBM)'s ICCAD 2015 contest starter_kit
- Paper reference: S. Do, M. Woo and S. Kang, "Fence-Region-Aware Mixed-Height Standard Cell Legalization", Proc. Great Lakes Symposium on VLSI, 2019, pp. 259-262. [(link)](https://dl.acm.org/citation.cfm?id=3318012)

### Features
- OpenDP has Tcl Interpreter using SWIG!
- Commercial format supports. (Si2 LEF/DEF parser has been ported).
- Fence region and multi-height legalization supports. (ICCAD 2017 Contest benchmarks)
- Fragmented ROW supports.
- Macro orientations supports.
