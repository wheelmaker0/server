#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <utility>
#include <unordered_map>
#include <absl/container/flat_hash_set.h>
#include <glog/logging.h>
#include "zookeeper/zookeeper.h"
#include "folly/synchronization/SaturatingSemaphore.h"

namespace zk_util{

class ZKClient{
public:
    class ChildrenWatcher{
    public:
        virtual void operator()(absl::flat_hash_set<std::string> &&children) = 0;
    };

    ZKClient(const std::string &host):host_(host),is_connected_(false){}

    bool connect();

    bool disconnect();

    bool is_connected();

    bool set(const std::string &path, const std::string &value);

    bool get(const std::string &path, std::string &value, bool watch = false);

    bool get_children(const std::string &path, std::vector<std::string> &children, bool watch = false);

    bool get_children_value(const std::string &path, absl::flat_hash_set<std::string> &children, bool watch = false);

    bool create(const std::string &path, std::string &out_real_path, const std::string &value);

    bool del(const std::string &path);

    bool exist(const std::string &path, bool watch = false);

    bool watch_children(const std::string &path, const std::shared_ptr<ChildrenWatcher> &watcher);

protected:
    std::string get_parent_path(std::string path);

    bool create_parent(const std::string &path);

    bool reconnect();

    void watch_function(int type, int state, const char* path);

    void watch_change_function(int type, int state, const char* path);

    static void watch_function_help(zhandle_t* zk, int type, int state, const char* path, void* arg);

    static void watch_change_function_help(zhandle_t* zk, int type, int state, const char* path, void* arg);

    std::mutex mutex_;
    zhandle_t* zk_ = nullptr;
    std::string host_;
    std::atomic<bool> is_connected_;
    folly::SaturatingSemaphore<true> is_connected_done_;
    std::unordered_map<std::string, std::weak_ptr<ChildrenWatcher>> watchers_;
};

};//namespace zk_util