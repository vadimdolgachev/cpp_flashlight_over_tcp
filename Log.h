#include <sstream>
#include <mutex>
#include <iostream>

#ifndef FLASHLIGHT_LOG_H
#define FLASHLIGHT_LOG_H

class Log : public std::ostringstream {
public:
    ~Log() override {
        std::lock_guard<std::mutex> lock(mMutex);
        std::cout << str();
    }

private:
    inline static std::mutex mMutex;
};

#endif //FLASHLIGHT_LOG_H
