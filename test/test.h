#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_for.h>

#include "concurrent_hash_map.h"
#include "common/time_util.h"
#include "common/util.h"
#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "boost/format.hpp"
#include "boost/core/demangle.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

class Test{
protected:
    std::string prog_name;
public:
    virtual ~Test(){}
    virtual std::string name(){
        return boost::core::demangle(typeid(*this).name());
    }

    virtual void run(int argc, char **argv){
        prog_name = fs::path(argv[0]).stem().string();
        po::options_description desc(boost::str(boost::format("Usage: %1% options\n  Allowed options") % prog_name));
        desc.add_options()("help,h", "show help message");
        add_options(desc);
        po::variables_map vm;
        try {
            po::store(po::parse_command_line(argc, argv, desc), vm);
            if (vm.count("help")) {
                std::cerr << desc << std::endl;
                return ;
            }
            po::notify(vm);
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            std::cerr << desc << std::endl;
            return ;
        }
        run();
    }

    virtual void add_options(po::options_description &desc) = 0;
    virtual void run() = 0;
};