// Reuse a bounded worker team across the output transform's small row bands.
// A call waits for all of its rows, including on allocation/task exceptions;
// captured pixel buffers can never outlive their caller.
#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace spkhost {
class RowWorkers {
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::packaged_task<void()>> jobs_;
    std::vector<std::thread> workers_;
    bool stopping_=false;
    void stop() noexcept {
        {std::lock_guard lock(mutex_);stopping_=true;}wake_.notify_all();
        for(auto& worker:workers_)if(worker.joinable())worker.join();
    }
public:
    RowWorkers() {
        // Leave CPU capacity for the GUI, decoder and driver. The submitting
        // thread also processes one partition; no task creates new threads.
        const unsigned total=std::min(8u,std::max(1u,std::thread::hardware_concurrency()/2));
        try {for(unsigned i=1;i<total;++i)workers_.emplace_back([this]{
            for(;;){
                std::packaged_task<void()> job;
                {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_||!jobs_.empty();});
                 if(stopping_&&jobs_.empty())return;job=std::move(jobs_.front());jobs_.pop_front();}
                job(); // packaged_task transports exceptions to the caller
            }
        });}catch(...){stop();throw;}
    }
    ~RowWorkers(){stop();}
    RowWorkers(const RowWorkers&)=delete;
    RowWorkers& operator=(const RowWorkers&)=delete;
    template<class F> void run(uint32_t rows,F&& body) {
        const unsigned count=std::min<unsigned>(workers_.size()+1,std::max(1u,rows/8));
        if(count<=1){body(0u,rows);return;}
        const uint32_t step=(rows+count-1)/count;
        std::vector<std::future<void>> pending;pending.reserve(count-1);
        struct WaitAll {std::vector<std::future<void>>& jobs;~WaitAll(){for(auto& job:jobs)if(job.valid())job.wait();}} finish{pending};
        for(unsigned i=1;i<count;++i){
            const uint32_t first=i*step,last=std::min(rows,first+step);
            if(first>=last)break;
            std::packaged_task<void()> job([&body,first,last]{body(first,last);});
            pending.push_back(job.get_future());
            try {{std::lock_guard lock(mutex_);jobs_.push_back(std::move(job));}}
            catch(...){pending.pop_back();throw;}
            wake_.notify_one();
        }
        body(0u,std::min(rows,step));
        for(auto& job:pending)job.get();
    }
};
inline RowWorkers& output_row_workers(){static RowWorkers team;return team;}
template<class F> void parallel_output_rows(uint32_t rows,F&& body){
    output_row_workers().run(rows,std::forward<F>(body));
}
}
