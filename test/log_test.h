#pragma once
#include "test.h"
#include "common/async_log.h"

class LogTest : public Test {
public:
    void run() override;
    virtual void add_options(po::options_description &desc) override{}
};