#include "lbMem.h"

#include <bitset>
#include <iostream>

using namespace std;

void printMem(circuit_state& state) {
  cout << "MEM" << endl;
  for (int i = 0; i < 10; i++) {
    cout << "\tm0$mem[" << i << "] = " << bitset<8>(state.m0$mem[i]) << endl;
  }

}

int main() {
  circuit_state state;
  state.self_clk = 1;
  state.self_clk_last = 0;

  state.self_wdata = 1;
  state.self_wen = 1;

  state.m0$raddr$reg0 = 1;
  state.m0$waddr$reg0 = 0;
  
  for (int i = 0; i < 10; i++) {
    state.m0$mem[i] = 0;
  }

  printMem(state);

  for (int i = 0; i < 40; i++) {
    simulate(&state);

    printMem(state);
    cout << "state.self_rdata             = " << bitset<8>(state.self_rdata) << endl;
    cout << "state.self_m0$raddr$reg0     = " << bitset<8>(state.m0$raddr$reg0) << endl;
    cout << "state.self_m0$waddr$reg0     = " << bitset<8>(state.m0$waddr$reg0) << endl;

    cout << "-----------------------------" << endl;
    //cout << "state->m0$raddr$reg0 = " << bitset<8>(state.m0$raddr$reg0) << endl;
  }

  if (state.self_rdata != 1) {
    return 1;
  }

  return 0;
}
