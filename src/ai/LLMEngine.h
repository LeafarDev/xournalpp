#pragma once

#include <string>

class LLMEngine {
public:
    ~LLMEngine();
    bool init(const std::string& modelPath);
    std::string run(const std::string& prompt);
    void shutdown();

    // Returns the actual context window size chosen at init time (0 if not yet initialised).
    int getContextSize() const;

private:
    struct Impl;
    Impl* impl = nullptr;
};
