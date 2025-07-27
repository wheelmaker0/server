
#include <string>
#include <mutex>
#include <cerrno>
#include <glog/logging.h>
#include <netinet/in.h>
#include "zk_client.h"

namespace zk_util{

bool ZKClient::connect(){
    std::unique_lock<std::mutex> lock(mutex_);
    return reconnect();
}

bool ZKClient::disconnect(){
    std::unique_lock<std::mutex> lock(mutex_);
    LOG(INFO) << "disconnect :" << zk_;
    google::FlushLogFiles(google::GLOG_INFO);
    if(zk_){
        return zookeeper_close(zk_) == ZOK;
    }
    return true;
}

bool ZKClient::is_connected(){
    return is_connected_;
}

bool ZKClient::set(const std::string &path, const std::string &value){
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    int error_code = ::zoo_set(zk_, path.c_str(), value.data(), value.size(), -1);
    if (error_code != ZOK) {
        LOG(ERROR) << "failed to set znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    return true;
}

bool ZKClient::get(const std::string &path, std::string &value, bool watch){
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    value.resize(1024);
    int buffer_len = value.size();
    int error_code;
    if(watch){
        error_code = ::zoo_wget(zk_, path.c_str(), &ZKClient::watch_change_function_help, this, value.data(), &buffer_len, nullptr);
    }else{
        error_code = ::zoo_get(zk_, path.c_str(), 0, value.data(), &buffer_len, nullptr);
    }
    if (error_code != ZOK || buffer_len <= 0) {
            LOG(ERROR) << "failed to get znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    value.resize(buffer_len);
    return true;
}


bool ZKClient::get_children(const std::string &path, std::vector<std::string> &children, bool watch){
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    struct String_vector output;
    int error_code;
    if(watch){
        error_code = ::zoo_wget_children(zk_, path.c_str(), &ZKClient::watch_change_function_help, this, &output);
    }else{
        error_code = ::zoo_get_children(zk_, path.c_str(), 0, &output);
    }
    if (error_code != ZOK) {
            LOG(ERROR) << "failed to get znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    children.reserve(output.count);
    for(int i = 0; i < output.count; i++){
        children.emplace_back(output.data[i]);
    }
    return true;
}

bool ZKClient::create(const std::string &path, std::string &out_real_path, const std::string &value){
    if(path.empty()){
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    if(!create_parent(path)){
        return false;
    }
    std::string zk_path = path;
    int mask = ZOO_EPHEMERAL;
    if (zk_path[zk_path.size()-1] == '*') {
        mask |= ZOO_SEQUENCE;
        if (zk_path[zk_path.rfind('/') + 1] != '*') {
            zk_path.erase(zk_path.size() - 1, 1);
        }
    }
    out_real_path.resize(1024);
    int error_code = ::zoo_create(zk_, zk_path.c_str(), value.data(), value.size(), 
                                &ZOO_OPEN_ACL_UNSAFE, mask,
                                out_real_path.data(),
                                out_real_path.size());
    if (error_code != ZOK) {
        LOG(ERROR) << "failed to create znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    out_real_path.resize(strlen(out_real_path.c_str()));
    return true;
}

bool ZKClient::del(const std::string &path){
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    int error_code = ::zoo_delete(zk_, path.c_str(), -1);
    if (error_code != ZOK) {
        LOG(ERROR) << "failed to delete znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    return true;
}

bool ZKClient::exist(const std::string &path, bool watch){
    std::unique_lock<std::mutex> lock(mutex_);
    if(!reconnect()){
        return false;
    }
    int error_code;
    if(watch){
        error_code = ::zoo_wexists(zk_, path.c_str(), &ZKClient::watch_change_function_help, this, nullptr);
    }else{
        error_code = ::zoo_exists(zk_, path.c_str(), 0, nullptr);
    }
    //明确的no node报错返回false
    if(error_code == ZNONODE){
        LOG(ERROR) << "failed to test exist znode " << host_ << path << " ,error: " << zerror(error_code);
        return false;
    }
    if (error_code != ZOK) {
        LOG(ERROR) << "failed to test exist znode " << host_ << path << " ,error: " << zerror(error_code);
        //此时网络不好，连不上zk，不一定掉了，如果强行重连也连不上。如果真掉了等网络好了也能检测到
        return true;
    }
    return true;
}

bool ZKClient::get_children_value(const std::string &path, absl::flat_hash_set<std::string> &children_value, bool watch){
    std::vector<std::string> children;
    if(!get_children(path, children, watch)){
        return false;
    }
    children_value.reserve(children.size());
    for(auto &child : children){
        std::string addr;
        if(get(std::string(path) + "/" + child, addr, watch)){
            children_value.insert(addr);
        }
    }
    return true;
}

bool ZKClient::create_parent(const std::string &path){
    size_t slash_pos = 0;
    while ((slash_pos = path.find('/', slash_pos + 1)) != std::string::npos) {
        std::string parent = path.substr(0, slash_pos);
        int error_code = ::zoo_create(zk_, parent.c_str(), nullptr, 0, &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
        if (error_code == ZOK) {
            LOG(INFO) << "created znode " << host_ << parent;
        } else if (error_code == ZNODEEXISTS) {
            // ok!
        } else {
            LOG(ERROR) << "failed to create znode " << host_ << parent << " ,error: " << zerror(error_code);
            return false;
        }
    }
    return true;
}

bool ZKClient::reconnect(){
    if(is_connected_ && zk_ != nullptr){
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        auto host_addr = zookeeper_get_connected_host(zk_, (struct sockaddr*) &addr, &addr_len);
        if(host_addr != NULL){
            return true;
        }
        else{
            LOG(INFO) << "zookeeper_get_connected_host null :" << host_;
            zookeeper_close(zk_);
            zk_ = NULL;
            is_connected_ = false;
        }
    }
    is_connected_done_.reset();
    zk_ = ::zookeeper_init(host_.c_str(), &ZKClient::watch_function_help,
                4000,    // recv_timeout 4s 此值过大，会导致实例宕机以后，zk
                        // session长时间不过期导致不能自动反注册
                nullptr, // client_id
                this,    // context
                0);
    if (zk_ == nullptr) {
        LOG(ERROR) << "failed to connect to zookeeper at " << host_ << ": " << zerror(errno);
        return false;
    }
    is_connected_done_.wait();
    LOG(INFO) << "reconnect success: " << zk_ << " at " << host_;
    return true;
}

void ZKClient::watch_function(int type, int state, const char* path){
    if (type != ZOO_SESSION_EVENT) {
        LOG(INFO) << "zoo_info type=" << type << " state=" << state << " is_connected=" << is_connected_;
        return;
    }
    if (state == ZOO_CONNECTING_STATE || state == ZOO_ASSOCIATING_STATE){
        return ;
    }
    is_connected_ = (state == ZOO_CONNECTED_STATE);
    is_connected_done_.post();
    LOG(INFO) << "zoo_info type=" << type << " state=" << state << " is_connected=" << is_connected_;
}

std::string ZKClient::get_parent_path(std::string path){
    size_t pos = path.find_last_of('/');
    if(pos != std::string::npos){
        return path.substr(0, pos);
    }
    return "";
}

bool ZKClient::watch_children(const std::string &path, const std::shared_ptr<ChildrenWatcher> &watcher){
    //先加watcher，这样不会丢消息，否则在get_children_value之后和加watcher之前的消息会丢失
    std::lock_guard<std::mutex> lock(mutex_);
    auto ret = watchers_.insert({path, watcher});
    if(!ret.second){
        return false;
    }
    mutex_.unlock();
    absl::flat_hash_set<std::string> children;
    get_children_value(path, children, true);
    watcher->operator()(std::move(children));
    return true;
}

void ZKClient::watch_change_function(int type, int state, const char* path){
    // LOG(INFO) << "watch_change_function type: " << type << " ,state: " << state << " ,path:" << path;
    std::string parent_path;
    if(type == ZOO_CHILD_EVENT){     //节点有增删
        parent_path = path;
    }else if(type == ZOO_CHANGED_EVENT){     //节点有更新
        parent_path = get_parent_path(path);
        LOG(INFO) << "parent_path: " << parent_path;
    }else{
        return ;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = watchers_.find(parent_path);
    //没有监控该节点
    if(iter == watchers_.end()){
        return ;
    }
    auto watcher = iter->second.lock();
    //必须提前释放，get_children_value内部用到了该锁
    mutex_.unlock();
    if(watcher){
        absl::flat_hash_set<std::string> children_value;
        get_children_value(parent_path, children_value, true);
        watcher->operator()(std::move(children_value));
    }
}

void ZKClient::watch_function_help(zhandle_t* zk, int type, int state, const char* path, void* arg){
    reinterpret_cast<ZKClient*>(arg)->watch_function(type, state, path);
}

void ZKClient::watch_change_function_help(zhandle_t* zk, int type, int state, const char* path, void* arg){
    reinterpret_cast<ZKClient*>(arg)->watch_change_function(type, state, path);
}

}; //namespace zk_util