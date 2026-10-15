// commonlib.umin/smin/umax/smax at any width. A CGRA built from a lake spec with
// data_width W maps min/max at W bits; these generators were 16-bit only
// (ASSERT "NYI non 16") although their bodies are width-generic.
#include <string>
#include <tuple>
#include "gtest/gtest.h"
#include "coreir.h"
#include "coreir/libs/commonlib.h"

using namespace CoreIR;

namespace {

class MinMaxWidth : public ::testing::TestWithParam<std::tuple<std::string, int>> {};

TEST_P(MinMaxWidth, GeneratesAtWidth) {
  const std::string op = std::get<0>(GetParam());
  const int width = std::get<1>(GetParam());
  const bool is_min = op.find("min") != std::string::npos;
  const std::string cmp = std::string(op[0] == 'u' ? "coreir.u" : "coreir.s") + (is_min ? "le" : "ge");

  Context* c = newContext();
  CoreIRLoadLibrary_commonlib(c);
  Module* m = c->getGenerator("commonlib." + op)->getModule({{"width", Const::make(c, width)}});
  ASSERT_NE(m, nullptr);
  RecordType* t = cast<RecordType>(m->getType());
  for (auto port : {"in0", "in1", "out"}) {
    EXPECT_EQ(cast<ArrayType>(t->getRecord().at(port))->getLen(), (uint)width) << port;
  }

  ASSERT_TRUE(m->runGenerator());
  ModuleDef* def = m->getDef();
  ASSERT_NE(def, nullptr);
  const auto& insts = def->getInstances();
  ASSERT_EQ(insts.size(), 2u);
  Instance* comp = insts.at(op[0] == 'u' ? "ucomp" : "scomp");
  Instance* mux = insts.at(is_min ? "min_mux" : "max_mux");
  EXPECT_EQ(comp->getModuleRef()->getRefName(), cmp);
  EXPECT_EQ(mux->getModuleRef()->getRefName(), "coreir.mux");
  for (Instance* i : {comp, mux}) {
    RecordType* it = cast<RecordType>(i->getModuleRef()->getType());
    EXPECT_EQ(cast<ArrayType>(it->getRecord().at("in0"))->getLen(), (uint)width) << i->getInstname();
  }
  deleteContext(c);
}

INSTANTIATE_TEST_SUITE_P(
  CommonlibMinMax,
  MinMaxWidth,
  ::testing::Combine(
    ::testing::Values("umin", "smin", "umax", "smax"),
    ::testing::Values(8, 16, 32, 64)));

}  // namespace
