#include "gtest/gtest.h"
#include "coreir.h"
#include "coreir/common/logging_lite.hpp"
#include "coreir/passes/analysis/coreirjson.h"

namespace CoreIR {
namespace {

TEST(InlineInstance, Basic) {
  Context* ctx = newContext();
  Module* top;

  if (!loadFromFile(ctx, "srcs/simple_hierarchy.json", &top)) ctx->die();
  assert(top != nullptr);
  ctx->setTop(top->getRefName());

  // Grab handles to Top.baz, Top.baz.bar.
  const auto inst_Top_baz = top->getDef()->getInstances().at("baz");
  const auto inst_Top_baz_bar = inst_Top_baz->getModuleRef()->
     getDef()->getInstances().at("bar");

  // Inline Top.baz.
  inlineInstance(inst_Top_baz);

  // Check that top has an instance of Foo (foo) and Bar (baz$bar).
  EXPECT_EQ(top->getDef()->getInstances().size(), 2);
  for (auto [name, inst] : top->getDef()->getInstances()) {
    if (name == "foo") {
      EXPECT_EQ(inst->getModuleRef()->getName(), "Foo");
    }
    else if(name == "baz$bar") {
      EXPECT_EQ(inst->getModuleRef(), inst_Top_baz_bar->getModuleRef());
    } else {
      EXPECT_TRUE(false) << "Unexpected instance";
    }
  }

  deleteContext(ctx);
}

}  // namespace
}  // namespace coreir

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
