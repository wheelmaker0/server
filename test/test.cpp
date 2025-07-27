#include "hash_test.h"
#include "file_diff.h"
#include "bin2str.h"
#include "faiss_test.h"
#include "compute.h"
#include "redis_test.h"
#include "log_test.h"
#include "feature_util_test.h"
#include "model_tools.h"
#include "get_libsvm.h"
#include "str_test.h"
#include "hdfs_test.h"
#include "zk_test.h"
#include "mysql_test.h"
#include "common/prometheus_client.h"
#include <glog/logging.h>


int main(int argn ,char **argv){

    google::InitGoogleLogging("total_recall_test");    
    google::SetStderrLogging(google::INFO);
    google::SetCommandLineOption("GLOG_minloglevel", "2");
    common::PrometheusClient::get_instance()->init(std::map<std::string, std::string>{ {"ip", "0.0.0.0"}, { "d", "1" } }, "tr", "10.30.23.35", "17500");
    
    std::vector<std::unique_ptr<Test>> tests;
    tests.emplace_back(new HashCollisionTest());
    tests.emplace_back(new RandMidHashCollisionTest());
    tests.emplace_back(new FidDiff());
    tests.emplace_back(new EbdDiff());
    tests.emplace_back(new IDDiff());
    tests.emplace_back(new RequestDiff());
    tests.emplace_back(new DebuggerDiff());
    tests.emplace_back(new Ebd2Str());
    tests.emplace_back(new PbGroupid2Str());
    tests.emplace_back(new PbGroupid2Json());
    tests.emplace_back(new ComputeIP());
    tests.emplace_back(new FaissTest());
    tests.emplace_back(new MakeFaissIndex());
    tests.emplace_back(new RecallRate());
    tests.emplace_back(new ScoreMid());
    tests.emplace_back(new RedisTest());
    tests.emplace_back(new FidFilter());
    tests.emplace_back(new EbdsDiff());
    tests.emplace_back(new Fid2Slot());
    tests.emplace_back(new GetLibsvm());
    tests.emplace_back(new MakeFid());
    tests.emplace_back(new GetMidFeature());
    tests.emplace_back(new GetUidFeature());
    tests.emplace_back(new GetU2UFeature());
    tests.emplace_back(new GetUserBehaviorFeature());
    tests.emplace_back(new LogTest());
    tests.emplace_back(new QueryWordPreprocess());
    tests.emplace_back(new MakeLibsvm());
    tests.emplace_back(new IniVerify());
    tests.emplace_back(new GetHdfsFile());
    tests.emplace_back(new ZKTest());
    tests.emplace_back(new GrpcZKTest());
    tests.emplace_back(new TotalRecallTest());
    tests.emplace_back(new MySqlClient());
    tests.emplace_back(new BruteEbdSearch());
    tests.emplace_back(new MergeEmbedding());
    tests.emplace_back(new TotalRecallResultDiff());
    tests.emplace_back(new EmbeddingTest());
    tests.emplace_back(new RedisCopy());
    tests.emplace_back(new ReadRedis());
    tests.emplace_back(new ReadUidRedis());

    std::vector<std::string> cmds;
    std::map<std::string, std::unique_ptr<Test>> tests_map;
    for(auto &t : tests){
        cmds.emplace_back(t->name());
        tests_map[t->name()] = std::move(t);
    }
    if(argn < 2){
        std::cerr << "cmd: " << common::container_2_str(cmds) << std::endl;
        return -1;
    }
    std::string cmd = argv[1];
    auto iter = tests_map.find(cmd);
    if(iter == tests_map.end()){
        std::cerr << "cmd: " << common::container_2_str(cmds) << std::endl;
        return -1;
    }
    argn--;
    argv++;
    iter->second->run(argn, argv);
    return 0;
}