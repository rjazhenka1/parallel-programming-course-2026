// Этап 1–2: реализация Scheduler, point() и await().
// Деструктор должен дождаться (join) всех запущенных потоков, в том числе после deadlock.
#include "scheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace sched {

thread_local Scheduler *Scheduler::current_ = nullptr;
thread_local Scheduler::Worker *Scheduler::worker_ = nullptr;

void Scheduler::spawn(std::function<void()> body) {
    workers_.push_back(std::make_unique<Worker>(std::move(body), static_cast<int>(workers_.size())));
}

void Scheduler::run(Strategy &strategy) {
    auto join_all = [&]() {
        for (auto &worker : workers_) {
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }
    };
    auto stop_all = [&]() {
        stopping_ = true;
        for (auto &worker : workers_) {
            if (worker->thread.joinable() && !worker->finished) {
                worker->turn.release();
            }
        }
        join_all();
    };

    try {
        for (auto &worker : workers_) {
            worker->thread = std::thread([this, worker = worker.get()]() {
                current_ = this;
                worker_ = worker;
                try {
                    worker->body();
                } catch (const Stopped &) {
                    return;
                } catch (...) {
                    worker->error = std::current_exception();
                }
                worker->finished = true;
                returned_.release();
            });
            returned_.acquire();
            if (worker->error) {
                std::rethrow_exception(worker->error);
            }
        }

        int last = -1;
        while (true) {
            std::vector<int> enabled;
            for (int i = 0; i < static_cast<int>(workers_.size()); i++) {
                if (!workers_[i]->finished && (!workers_[i]->condition || workers_[i]->condition())) {
                    enabled.push_back(i);
                }
            }
            if (enabled.empty()) {
                for (auto &worker : workers_) {
                    if (!worker->finished) {
                        deadlock = "held={}";
                        stop_all();
                        break;
                    }
                }
                break;
            }

            int next = strategy.pick(enabled, last);
            if (std::find(enabled.begin(), enabled.end(), next) == enabled.end()) {
                throw std::logic_error("strategy picked a disabled worker");
            }
            trace_ += static_cast<char>('A' + next);
            last = next;
            workers_[next]->turn.release();
            returned_.acquire();
            if (workers_[next]->error) {
                std::rethrow_exception(workers_[next]->error);
            }
        }
    } catch (...) {
        stop_all();
        throw;
    }
    join_all();
}

std::string Scheduler::trace_string() const { return trace_; }

void Scheduler::pause() {
    worker_->touched = false;
    returned_.release();
    worker_->turn.acquire();
    if (stopping_) {
        throw Stopped{};
    }
}

void Scheduler::before_access() {
    if (worker_->touched) {
        current_->pause();
    }
    worker_->touched = true;
}

void Scheduler::log_operation(const std::string &operation) {
    if (current_->logging) {
        current_->log.push_back(std::string(1, 'A' + worker_->id) + "  " + operation);
    }
}

void point() {
    Scheduler::worker_->touched = true;
    Scheduler::log_operation("window");
}

void await(std::function<bool()> cond) {
    auto &worker = *Scheduler::worker_;
    if (worker.touched || !cond()) {
        worker.condition = std::move(cond);
        Scheduler::current_->pause();
        worker.condition = {};
    }
    worker.touched = true;
    Scheduler::log_operation("await done");
}

} // namespace sched
