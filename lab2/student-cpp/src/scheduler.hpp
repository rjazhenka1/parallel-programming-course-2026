#pragma once
// Этап 1: планировщик, который ведёт настоящие потоки строго по одному (передача хода через семафоры).
// Этап 2: обнаружение deadlock в главном цикле и корректное завершение застрявших потоков.
// Внутреннее устройство (поля, семафоры, вспомогательные функции для shared и sched_mutex) придумываете сами.

#include <exception>
#include <functional>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "strategy.hpp"

namespace sched {

class Scheduler {
    struct Worker {
        Worker(std::function<void()> fn, int id) : body(std::move(fn)), id(id) {}

        std::function<void()> body;
        int id;
        std::thread thread;
        std::binary_semaphore turn{0};
        bool finished = false;
        bool touched = true;
        std::exception_ptr error;
        std::function<bool()> condition;
    };

    struct Stopped {};

    std::vector<std::unique_ptr<Worker>> workers_;
    std::string trace_;
    std::binary_semaphore returned_{0};
    bool stopping_ = false;
    static thread_local Scheduler *current_;
    static thread_local Worker *worker_;

    void pause();
    friend void point();
    friend void await(std::function<bool()> cond);

  public:
    Scheduler() = default;
    ~Scheduler() = default;
    // Зарегистрировать поток. Потоки получают буквы A, B, C… в порядке вызова spawn.
    void spawn(std::function<void()> body);
    // Запустить потоки и вести их по одному: на каждой паузе спросить strategy.pick(enabled, last).
    void run(Strategy &strategy);
    // Трасса прогона: буквы потоков, выбранных во всех решениях подряд, например "AAAABB".
    std::string trace_string() const;
    static void before_access();
    static void log_operation(const std::string &operation);

    // Если true, каждая операция с общими данными добавляет строку в log. Выставляет runner.
    bool logging = false;
    // Лог прогона: строки "<поток>  <операция>", например "A  lock mu", "A  balance -> 100".
    std::vector<std::string> log;
    // Пустая строка, если deadlock не было; иначе "held={a:A, b:B}" (этап 2).
    std::string deadlock;
};

// window() из тестовых программ: пауза перед следующей операцией с общими данными обязательна.
void point();
// Ждать, пока cond не станет true. До этого поток не попадает в enabled.
void await(std::function<bool()> cond);

} // namespace sched
