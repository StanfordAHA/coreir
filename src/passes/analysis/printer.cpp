#include "coreir.h"
#include "coreir/passes/analysis/printer.h"
#include "coreir/passes/analysis/coreirjson.h"

using namespace std;
using namespace CoreIR;

bool Passes::Printer::runOnContext(Context* c) {
  cout << "Printer!\n";
  if (c->hasTop()) {
    getAnalysisPass<Passes::CoreIRJson>("json")->writeToStream(cout,c->getTop()->getRefName());
  }
  else {
    getAnalysisPass<Passes::CoreIRJson>("json")->writeToStream(cout,"");
  }
  cout << endl << endl;
  return false;
}

