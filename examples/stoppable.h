#ifndef __STOPPABLE_H_
#define __STOPPABLE_H_

#include <future>
#include <iostream>

class Stoppable {
public:
    Stoppable() :
    futureObj(exitSignal.get_future()) {

    }

    Stoppable(Stoppable && obj) :
    exitSignal(std::move(obj.exitSignal)), futureObj(std::move(obj.futureObj)) {
        std::cout << "Move Constructor is called" << std::endl;
    }

    Stoppable & operator=(Stoppable && obj) {
        std::cout << "Move Assignment is called" << std::endl;
        exitSignal = std::move(obj.exitSignal);
        futureObj = std::move(obj.futureObj);
        return *this;
    }

    virtual void run() = 0;

    void operator()() {
        run();
    }

    bool stopRequested() {
        if (futureObj.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::timeout)
            return false;
        return true;
    }

    void stop() {
        exitSignal.set_value();
    }

private:
    std::promise<void> exitSignal;
    std::future<void> futureObj;
};

#endif // __STOPPABLE_H_
