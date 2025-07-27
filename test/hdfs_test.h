#pragma once

#include "test.h"

class GetHdfsFile : public Test {
public:
protected:
    std::string remote_path;
    std::string local_path;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("remote_path,r", po::value<std::string>(&remote_path)->required(), "remote_path")
        ("local_path,l", po::value<std::string>(&local_path)->required(), "local_path");
    }

    virtual void run() override;
};