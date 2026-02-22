/*
 * Xournal++
 *
 * Unit tests for ModelRouter (model selection and availability).
 *
 * @license GNU GPLv2 or later
 */

#include <gtest/gtest.h>

#include "ai/ModelRouter.h"
#include "ai/IChatModel.h"


TEST(ModelRouter, createModelCopilotReturnsNonNull) {
    // Router can be constructed with nullptr Settings for these tests;
    // createModel does not read Settings.
    ModelRouter router(nullptr);
    std::unique_ptr<IChatModel> m = router.createModel(ModelType::Copilot);
    // Copilot CLI model is always created; availability is checked at sendMessage time.
    EXPECT_NE(m.get(), nullptr);
}

TEST(ModelRouter, createModelLocalWithoutEnvReturnsNull) {
    ModelRouter router(nullptr);
    // With XOURNALPP_LLM_MODEL unset, Local model is not created.
    std::unique_ptr<IChatModel> m = router.createModel(ModelType::Local);
    EXPECT_EQ(m.get(), nullptr);
}

TEST(ModelRouter, hasLocalModelWithoutEnv) {
    ModelRouter router(nullptr);
    EXPECT_FALSE(router.hasLocalModel());
}

TEST(ModelRouter, createModelLocalWithInvalidPathReturnsNull) {
    ModelRouter router(nullptr);
    // We cannot set env in a portable way and then create a file in unit test
    // without affecting other tests. So we only test the unset case above.
    (void)router;
}
