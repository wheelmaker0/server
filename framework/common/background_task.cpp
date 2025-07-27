#include "background_task.h"
#include "thread_util/thread_util.h"

namespace common{

void BackgroundTaskManager::loop(){
    thread_util::setThreadScheduleLowPriority();
    while(!finished_){
        std::weak_ptr<BackgroundTask> task;
        while(regist_queue_.try_pop(task)){
            task_queue_.emplace_back(task);
        }
        for(auto iter = task_queue_.begin(); iter != task_queue_.end(); ){
            auto p = iter->lock();
            if(p == nullptr){
                iter = task_queue_.erase(iter);
            }
            else{
                p->run();
                ++iter;
            }
        }
        sleep(1);
    }
}

} //common