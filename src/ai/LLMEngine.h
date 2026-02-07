#pragma once

#include <string>

class LLMEngine {
public:
    bool init(const std::string& modelPath);
    std::string run(const std::string& prompt);
    void shutdown();

private:
    struct Impl;
    Impl* impl = nullptr;
};
