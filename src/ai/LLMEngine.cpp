#include "ai/LLMEngine.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#include <glib.h>

#include "ggml-backend.h"
#include "llama.h"

struct LLMEngine::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    int n_ctx = 2048;
    int n_threads = 4;
    int n_batch = 2048;
};

static std::vector<llama_token> tokenize_prompt(const llama_vocab* vocab, const std::string& prompt) {
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), static_cast<int>(prompt.size()), nullptr, 0, true, true);
    if (n_tokens < 0) {
        n_tokens = -n_tokens;
    }
    std::vector<llama_token> tokens(static_cast<size_t>(n_tokens));
    if (n_tokens > 0) {
        llama_tokenize(vocab, prompt.c_str(), static_cast<int>(prompt.size()), tokens.data(), n_tokens, true, true);
    }
    return tokens;
}

static std::string token_to_piece(const llama_vocab* vocab, llama_token token) {
    std::string piece;
    piece.resize(32);
    int n = llama_token_to_piece(vocab, token, piece.data(), static_cast<int>(piece.size()), 0, true);
    if (n < 0) {
        piece.resize(static_cast<size_t>(-n));
        n = llama_token_to_piece(vocab, token, piece.data(), static_cast<int>(piece.size()), 0, true);
    }
    if (n > 0) {
        piece.resize(static_cast<size_t>(n));
    } else {
        piece.clear();
    }
    return piece;
}

// ---------------------------------------------------------------------------
// Dynamic resource helpers
// ---------------------------------------------------------------------------

// Returns the best estimate of free device memory in bytes.
// Prefers GPU/iGPU devices (Metal on Apple Silicon = unified memory GPU view).
// Falls back to the CPU device if no GPU is present.
static size_t queryFreeDeviceMemoryBytes() {
    size_t bestFree = 0;
    bool foundGpu = false;
    size_t count = ggml_backend_dev_count();
    for (size_t i = 0; i < count; ++i) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        enum ggml_backend_dev_type dtype = ggml_backend_dev_type(dev);
        size_t free = 0, total = 0;
        ggml_backend_dev_memory(dev, &free, &total);
        if (dtype == GGML_BACKEND_DEVICE_TYPE_GPU || dtype == GGML_BACKEND_DEVICE_TYPE_IGPU) {
            if (!foundGpu || free > bestFree) {
                bestFree = free;
                foundGpu = true;
            }
        }
    }
    if (!foundGpu) {
        // No discrete GPU: fall back to CPU/host memory report.
        for (size_t i = 0; i < count; ++i) {
            ggml_backend_dev_t dev = ggml_backend_dev_get(i);
            if (ggml_backend_dev_type(dev) == GGML_BACKEND_DEVICE_TYPE_CPU) {
                size_t free = 0, total = 0;
                ggml_backend_dev_memory(dev, &free, &total);
                if (free > bestFree) { bestFree = free; }
            }
        }
    }
    return bestFree;
}

// Compute the optimal context window size based on model architecture and
// available device memory.  The KV-cache budget drives the upper bound:
//   kvBytesPerToken = 2 (K+V) × n_layers × n_head_kv × head_dim × 2 (f16)
// We use 50 % of free device memory so that model weights, activations and
// the OS still have breathing room.
static int computeOptimalContext(llama_model* model) {
    const int nCtxTrain = llama_model_n_ctx_train(model);
    const int nLayers   = llama_model_n_layer(model);
    const int nEmbd     = llama_model_n_embd(model);
    const int nHead     = llama_model_n_head(model);
    const int nHeadKv   = llama_model_n_head_kv(model);

    // head_dim = total embedding / number of heads
    const int headDim = (nHead > 0) ? (nEmbd / nHead) : 64;

    // KV cache: 2 bytes (f16) × 2 (K and V) × layers × kv_heads × head_dim
    const size_t kvBytesPerToken =
            static_cast<size_t>(4) *
            static_cast<size_t>(nLayers > 0 ? nLayers : 1) *
            static_cast<size_t>(nHeadKv > 0 ? nHeadKv : 1) *
            static_cast<size_t>(headDim > 0 ? headDim : 64);

    const size_t freeMem = queryFreeDeviceMemoryBytes();
    // Reserve 50 % of free memory for KV cache; the rest goes to weights,
    // activations and the OS.
    const size_t kvBudget = freeMem / 2;

    int optCtx = 0;
    if (kvBytesPerToken > 0 && kvBudget > 0) {
        optCtx = static_cast<int>(kvBudget / kvBytesPerToken);
    }

    // Clamp to training context and a hard cap.
    constexpr int kMaxCtx = 32768;
    if (optCtx <= 0 || optCtx > nCtxTrain) { optCtx = nCtxTrain; }
    if (optCtx > kMaxCtx) { optCtx = kMaxCtx; }

    // Round down to multiple of 512; enforce a sensible minimum.
    optCtx = (optCtx / 512) * 512;
    if (optCtx < 2048) { optCtx = 2048; }

    g_debug("[llm] dynamic ctx: n_ctx=%d  (train=%d, layers=%d, kv_heads=%d, "
            "head_dim=%d, kvBytes/tok=%zu, freeMem=%zu MB)",
            optCtx, nCtxTrain, nLayers, nHeadKv, headDim,
            kvBytesPerToken, freeMem / (1024 * 1024));

    return optCtx;
}

// ---------------------------------------------------------------------------

LLMEngine::~LLMEngine() { shutdown(); }

bool LLMEngine::init(const std::string& modelPath) {
    if (impl != nullptr) {
        return true;
    }

    llama_backend_init();

    auto model_params = llama_model_default_params();
    llama_model* model = llama_model_load_from_file(modelPath.c_str(), model_params);
    if (!model) {
        return false;
    }

    // --- Dynamic context and thread sizing ---
    const int optCtx = computeOptimalContext(model);
    const int nThreads = std::max(1, std::min(static_cast<int>(std::thread::hardware_concurrency()), 8));

    auto ctx_params = llama_context_default_params();
    ctx_params.n_ctx = static_cast<uint32_t>(optCtx);
    ctx_params.n_batch = ctx_params.n_ctx;
    ctx_params.n_threads = nThreads;
    ctx_params.n_threads_batch = nThreads;

    llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        llama_model_free(model);
        return false;
    }

    impl = new Impl{};
    impl->model = model;
    impl->ctx = ctx;
    impl->vocab = llama_model_get_vocab(model);
    impl->n_ctx = static_cast<int>(ctx_params.n_ctx);
    impl->n_threads = static_cast<int>(ctx_params.n_threads);
    impl->n_batch = static_cast<int>(ctx_params.n_batch);
    return true;
}

std::string LLMEngine::run(const std::string& prompt) {
    if (!impl || !impl->model || !impl->ctx) {
        return {};
    }

    auto tokens = tokenize_prompt(impl->vocab, prompt);
    if (tokens.empty()) {
        return {};
    }

    const int maxTokens = std::max(1, std::min(impl->n_ctx, impl->n_batch));
    if (static_cast<int>(tokens.size()) > maxTokens) {
        tokens.erase(tokens.begin(), tokens.end() - maxTokens);
    }

    llama_batch batch = llama_batch_init(static_cast<int>(tokens.size()), 0, 1);
    batch.n_tokens = static_cast<int>(tokens.size());
    for (int i = 0; i < batch.n_tokens; ++i) {
        batch.token[i] = tokens[static_cast<size_t>(i)];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == batch.n_tokens - 1);
    }

    if (llama_decode(impl->ctx, batch) != 0) {
        llama_batch_free(batch);
        return {};
    }
    llama_batch_free(batch);

    constexpr int n_predict = 768;
    std::string output;
    output.reserve(512);

    for (int i = 0; i < n_predict; ++i) {
        const float* logits = llama_get_logits(impl->ctx);
        const int n_vocab = llama_vocab_n_tokens(impl->vocab);

        int best_id = 0;
        float best_logit = logits[0];
        for (int t = 1; t < n_vocab; ++t) {
            if (logits[t] > best_logit) {
                best_logit = logits[t];
                best_id = t;
            }
        }

        llama_token token = static_cast<llama_token>(best_id);
        if (token == llama_vocab_eos(impl->vocab)) {
            break;
        }

        output += token_to_piece(impl->vocab, token);

        llama_batch next = llama_batch_init(1, 0, 1);
        next.n_tokens = 1;
        next.token[0] = token;
        next.pos[0] = static_cast<int>(tokens.size()) + i;
        next.n_seq_id[0] = 1;
        next.seq_id[0][0] = 0;
        next.logits[0] = true;

        if (llama_decode(impl->ctx, next) != 0) {
            llama_batch_free(next);
            break;
        }
        llama_batch_free(next);
    }

    return output;
}

int LLMEngine::getContextSize() const {
    return impl ? impl->n_ctx : 0;
}

void LLMEngine::shutdown() {
    if (!impl) {
        return;
    }

    if (impl->ctx) {
        llama_free(impl->ctx);
    }
    if (impl->model) {
        llama_model_free(impl->model);
    }
    llama_backend_free();

    delete impl;
    impl = nullptr;
}
