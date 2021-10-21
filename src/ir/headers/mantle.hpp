// This file is just included in context.cpp

ArrayType* _get_array_type_arg(Values args, std::string name) {
  Type* t = args.at(name)->get<Type*>();
  ASSERT(isa<ArrayType>(t), name + "t1 needs to be an array");
  return cast<ArrayType>(t);
}

Namespace* CoreIRLoadHeader_mantle(Context* c) {

  Namespace* mantle = c->newNamespace("mantle");

  // Change the name of rst and clr
  auto regFun = [](Context* c, Values args) {
    uint width = args.at("width")->get<int>();
    bool en = args.at("has_en")->get<bool>();
    bool clr = args.at("has_clr")->get<bool>();
    bool rst = args.at("has_rst")->get<bool>();
    assert(!(clr && rst));
    Type* ptype = c->Bit()->Arr(width);

    RecordParams r({
      {"in", ptype->getFlipped()},
      {"clk", c->Named("coreir.clkIn")},
      {"out", ptype},
    });
    if (en) r.push_back({"en", c->BitIn()});
    if (clr) r.push_back({"clr", c->BitIn()});
    if (rst) r.push_back({"rst", c->Named("coreir.arstIn")});
    return c->Record(r);
  };

  auto regModParamFun =
    [](Context* c, Values genargs) -> std::pair<Params, Values> {
    Params modparams;
    Values defaultargs;
    // if (genargs.at("has_rst")->get<bool>() ||
    // genargs.at("has_clr")->get<bool>()) {
    int width = genargs.at("width")->get<int>();
    modparams["init"] = BitVectorType::make(c, width);
    defaultargs["init"] = Const::make(c, width, 0);
    //}
    return {modparams, defaultargs};
  };

  Params regGenParams(
    {{"width", c->Int()},
     {"has_en", c->Bool()},
     {"has_clr", c->Bool()},
     {"has_rst", c->Bool()}});
  TypeGen* regTypeGen = mantle->newTypeGen("regType", regGenParams, regFun);

  auto reg = mantle->newGeneratorDecl("reg", regTypeGen, regGenParams);
  reg->setModParamsGen(regModParamFun);
  reg->addDefaultGenArgs(
    {{"has_en", Const::make(c, false)},
     {"has_clr", Const::make(c, false)},
     {"has_rst", Const::make(c, false)}});

  reg->setGeneratorDefFromFun([](Context* c, Values genargs, ModuleDef* def) {
    int width = genargs.at("width")->get<int>();
    bool en = genargs.at("has_en")->get<bool>();
    bool clr = genargs.at("has_clr")->get<bool>();
    bool rst = genargs.at("has_rst")->get<bool>();

    auto io = def->getInterface();
    Values wval({{"width", Const::make(c, width)}});

    Wireable* reg;
    if (rst) {
      reg = def->addInstance(
        "reg0",
        "coreir.reg_arst",
        wval,
        {{"init", def->getModule()->getArg("init")}});
      def->connect("reg0.rst", "self.rst");
    }
    else {
      reg = def->addInstance(
        "reg0",
        "coreir.reg",
        wval,
        {{"init", def->getModule()->getArg("init")}});
    }
    def->connect("reg0.out", "self.out");
    def->connect("reg0.clk", "self.clk");
    Wireable* toIn = reg->sel("in");

    if (clr) {
      auto mux = def->addInstance("clrMux", "coreir.mux", wval);
      auto zero = def->addInstance(
        "c0",
        "coreir.const",
        wval,
        {{"value", Const::make(c, width, 0)}});
      def->connect(mux->sel("out"), toIn);
      def->connect(mux->sel("in1"), zero->sel("out"));
      def->connect(mux->sel("sel"), io->sel("clr"));
      toIn = mux->sel("in0");
    }

    if (en) {
      auto mux = def->addInstance("enMux", "coreir.mux", wval);
      def->connect(mux->sel("out"), toIn);
      def->connect(mux->sel("in0"), reg->sel("out"));
      def->connect(mux->sel("sel"), io->sel("en"));
      toIn = mux->sel("in1");
    }
    def->connect(io->sel("in"), toIn);
  });

  // Add

  auto addFun = [](Context* c, Values args) {
    int width = args.at("width")->get<int>();
    bool cin = args.at("has_cin")->get<bool>();
    bool cout = args.at("has_cout")->get<bool>();
    RecordParams r({
      {"in0", c->BitIn()->Arr(width)},
      {"in1", c->BitIn()->Arr(width)},
      {"out", c->Bit()->Arr(width)},
    });
    if (cin) r.push_back({"cin", c->BitIn()});
    if (cout) r.push_back({"cout", c->Bit()});
    return c->Record(r);
  };

  Params addGenParams({
    {"width", c->Int()},
    {"has_cin", c->Bool()},
    {"has_cout", c->Bool()},
  });
  TypeGen* addTypeGen = mantle->newTypeGen("addType", addGenParams, addFun);

  auto add = mantle->newGeneratorDecl("add", addTypeGen, addGenParams);
  add->addDefaultGenArgs(
    {{"has_cin", Const::make(c, false)}, {"has_cout", Const::make(c, false)}});
  auto sub = mantle->newGeneratorDecl("sub", addTypeGen, addGenParams);
  sub->addDefaultGenArgs(
    {{"has_cin", Const::make(c, false)}, {"has_cout", Const::make(c, false)}});

  // Add "wire"
  Params wireParams({
    {"type", CoreIRType::make(c)},
  });
  TypeGen* wireTG = mantle->newTypeGen(
    "wire",
    wireParams,
    [](Context* c, Values args) {
      Type* t = args.at("type")->get<Type*>();
      return c->Record({{"in", t->getFlipped()}, {"out", t}});
    });
  Generator* wire = mantle->newGeneratorDecl("wire", wireTG, wireParams);
  wire->setGeneratorDefFromFun([](Context* c, Values genargs, ModuleDef* def) {
    def->connect("self.in", "self.out");
  });

  wire->setPrimitiveExpressionLambda([]() { return vAST::make_id("in"); });

  Params counterParams(
    {{"width", c->Int()},
     {"has_en", c->Bool()},
     {"has_srst", c->Bool()},
     {"has_max", c->Bool()}});
  // counter type
  mantle->newTypeGen(
    "counter_type",  // name for the typegen
    counterParams,
    [](Context* c, Values genargs) {  // Function to compute type
      uint width = genargs.at("width")->get<int>();
      uint has_en = genargs.at("has_en")->get<bool>();
      uint has_srst = genargs.at("has_srst")->get<bool>();
      RecordParams r(
        {{"clk", c->Named("coreir.clkIn")}, {"out", c->Bit()->Arr(width)}});
      if (has_en) r.push_back({"en", c->BitIn()});
      if (has_srst) r.push_back({"srst", c->BitIn()});
      return c->Record(r);
    });
  auto counterModParamFun =
    [](Context* c, Values genargs) -> std::pair<Params, Values> {
    Params modparams;
    Values defaultargs;
    uint width = genargs.at("width")->get<int>();
    bool has_max = genargs.at("has_max")->get<bool>();
    modparams["init"] = BitVectorType::make(c, width);
    defaultargs["init"] = Const::make(c, BitVector(width, 0));
    if (has_max) { modparams["max"] = BitVectorType::make(c, width); }
    return {modparams, defaultargs};
  };

  Generator* counter = mantle->newGeneratorDecl(
    "counter",
    mantle->getTypeGen("counter_type"),
    counterParams);
  counter->setModParamsGen(counterModParamFun);
  counter->addDefaultGenArgs(
    {{"has_max", Const::make(c, false)},
     {"has_en", Const::make(c, false)},
     {"has_srst", Const::make(c, false)}});
  counter->setGeneratorDefFromFun(
    [](Context* c, Values genargs, ModuleDef* def) {
      uint width = genargs.at("width")->get<int>();
      bool has_max = genargs.at("has_max")->get<bool>();
      bool has_en = genargs.at("has_en")->get<bool>();
      bool has_srst = genargs.at("has_srst")->get<bool>();
      Values widthParams({{"width", Const::make(c, width)}});
      def->addInstance(
        "r",
        "mantle.reg",
        {{"width", Const::make(c, width)},
         {"has_en", Const::make(c, has_en)},
         {"has_clr", Const::make(c, has_srst)}},
        {{"init", def->getModule()->getArg("init")}});
      def->connect("self.clk", "r.clk");
      if (has_en) { def->connect("self.en", "r.en"); }
      if (has_srst) { def->connect("self.srst", "r.clr"); }
      def->addInstance(
        "c1",
        "coreir.const",
        widthParams,
        {{"value", Const::make(c, width, 1)}});
      def->addInstance("add", "coreir.add", widthParams);
      def->connect("r.out", "add.in0");
      def->connect("c1.out", "add.in1");
      def->connect("r.out", "self.out");
      if (has_max) {
        def->addInstance(
          "c0",
          "coreir.const",
          widthParams,
          {{"value", Const::make(c, width, 0)}});
        def->addInstance("mux", "coreir.mux", widthParams);
        def->addInstance("eq", "coreir.eq", widthParams);
        def->addInstance(
          "maxval",
          "coreir.const",
          widthParams,
          {{"value", def->getModule()->getArg("max")}});
        def->connect("r.out", "eq.in0");
        def->connect("maxval.out", "eq.in1");
        def->connect("eq.out", "mux.sel");
        def->connect("add.out", "mux.in0");
        def->connect("c0.out", "mux.in1");
        def->connect("mux.out", "r.in");
      }
      else {
        def->connect("add.out", "r.in");
      }
    });

  {  // Clock en register (no reset)
    auto regCEFun = [](Context* c, Values args) {
      uint width = args.at("width")->get<int>();
      Type* ptype = c->Bit()->Arr(width);

      RecordParams r({
        {"in", ptype->getFlipped()},
        {"ce", c->BitIn()},
        {"out", ptype},
        {"clk", c->Named("coreir.clkIn")},
      });
      return c->Record(r);
    };
    Params regCEGenParams({{"width", c->Int()}});
    TypeGen* regCETypeGen = mantle->newTypeGen(
      "regCEType",
      regCEGenParams,
      regCEFun);
    auto regCE = mantle->newGeneratorDecl(
      "regCE",
      regCETypeGen,
      regCEGenParams);
    json vjson;
    vjson["interface"] = {
      "input [width-1:0] in",
      "input ce",
      "output [width-1:0] out",
      "input clk"};
    vjson["definition"] =
      ""
      "  reg [width-1:0] value;\n"
      "  always @(posedge clk) begin\n"
      "    if (ce) begin\n"
      "      value <= in;\n"
      "    end\n"
      "  end\n"
      "  assign out = value;";

    regCE->getMetaData()["verilog"] = vjson;
  }

  {  // Clock en register with reset
    auto regCEArstFun = [](Context* c, Values args) {
      uint width = args.at("width")->get<int>();
      Type* ptype = c->Bit()->Arr(width);

      RecordParams r({
        {"in", ptype->getFlipped()},
        {"ce", c->BitIn()},
        {"out", ptype},
        {"clk", c->Named("coreir.clkIn")},
        {"arst", c->Named("coreir.arstIn")},
      });
      return c->Record(r);
    };
    auto regModParamFun =
      [](Context* c, Values genargs) -> std::pair<Params, Values> {
      Params modparams;
      Values defaultargs;
      int width = genargs.at("width")->get<int>();
      modparams["init"] = BitVectorType::make(c, width);
      defaultargs["init"] = Const::make(c, width, 0);
      //}
      return {modparams, defaultargs};
    };

    Params regCEArstGenParams({{"width", c->Int()}});
    TypeGen* regCEArstTypeGen = mantle->newTypeGen(
      "regCEArstType",
      regCEArstGenParams,
      regCEArstFun);
    auto regCEArst = mantle->newGeneratorDecl(
      "regCE_arst",
      regCEArstTypeGen,
      regCEArstGenParams);
    regCEArst->setModParamsGen(regModParamFun);
    json vjson;
    vjson["parameters"] = {"init"};
    vjson["interface"] = {
      "input [width-1:0] in",
      "input ce",
      "output [width-1:0] out",
      "input clk",
      "input arst",
    };
    vjson["definition"] =
      ""
      "  reg [width-1:0] value;\n"
      "  always @(posedge clk, posedge arst) begin\n"
      "    if (arst) begin\n"
      "      value <= init;\n"
      "    end\n"
      "    else if (ce) begin\n"
      "      value <= in;\n"
      "    end\n"
      "  end\n"
      "  assign out = value;";
    regCEArst->getMetaData()["verilog"] = vjson;
  }

  {
    Params concatArrTParams(
        {{"t0", CoreIRType::make(c)}, {"t1", CoreIRType::make(c)}});

    auto concatArrTTypeGen = mantle->newTypeGen(
        "concatArrTTypeFun", concatArrTParams, [](Context* c, Values args) {
          ArrayType* t0_arr = _get_array_type_arg(args, "t0");
          ArrayType* t1_arr = _get_array_type_arg(args, "t1");
          ASSERT(t0_arr->getElemType() == t1_arr->getElemType(),
                 "t0 and t1 need the same element type");
          ArrayType* t2 = c->Array(t0_arr->getLen() + t1_arr->getLen(),
                                   t0_arr->getElemType()->getFlipped());
          return c->Record({{"in0", t0_arr}, {"in1", t1_arr}, {"out", t2}});
        });
    Generator* concatArrT =
        mantle->newGeneratorDecl("concatArrT", concatArrTTypeGen, concatArrTParams);

    concatArrT->setGeneratorDefFromFun(
        [](Context* c, Values genargs, ModuleDef* def) {
          ArrayType* t0_arr = _get_array_type_arg(genargs, "t0");
          ArrayType* t1_arr = _get_array_type_arg(genargs, "t1");

          unsigned int t0_len = t0_arr->getLen();
          for (unsigned int i = 0; i < t0_len; i++) {
            def->connect("self.in0." + toString(i), "self.out." + toString(i));
          }
          for (unsigned int i = 0; i < t1_arr->getLen(); i++) {
            def->connect("self.in1." + toString(i),
                         "self.out." + toString(t0_len + i));
          }
        });
  }

  {
    Params sliceArrTParams(
      {{"t", CoreIRType::make(c)}, {"lo", c->Int()}, {"hi", c->Int()}});

    auto sliceArrTTypeGen = mantle->newTypeGen(
      "sliceArrTTypeFun",
      sliceArrTParams,
      [](Context* c, Values args) {
        ArrayType* t_arr = _get_array_type_arg(args, "t");
        uint lo = args.at("lo")->get<int>();
        uint hi = args.at("hi")->get<int>();
        ASSERT(
          lo < hi && hi <= t_arr->getLen(),
          "Bad slice args! lo=" + to_string(lo) + ", hi=" + to_string(hi));

        ArrayType* t_out = c->Array(
          hi - lo,
          t_arr->getElemType()->getFlipped());
        return c->Record({{"in", t_arr}, {"out", t_out}});
      });
    Generator* sliceArrT = mantle->newGeneratorDecl(
      "sliceArrT",
      sliceArrTTypeGen,
      sliceArrTParams);

    sliceArrT->setGeneratorDefFromFun(
      [](Context* c, Values genargs, ModuleDef* def) {
        uint lo = genargs.at("lo")->get<int>();
        uint hi = genargs.at("hi")->get<int>();

        for (uint i = 0; i < (hi - lo); i++) {
          def->connect(
            "self.in." + toString(lo + i),
            "self.out." + toString(i));
        }
      });
  }

  {
    Params getArrTParams({{"t", CoreIRType::make(c)}, {"i", c->Int()}});

    auto getArrTTypeGen = mantle->newTypeGen(
      "getArrTTypeFun",
      getArrTParams,
      [](Context* c, Values args) {
        ArrayType* t_arr = _get_array_type_arg(args, "t");
        uint i = args.at("i")->get<int>();
        ASSERT(i <= t_arr->getLen(), "Bad get args! i=" + to_string(i));

        Type* t_out = t_arr->getElemType()->getFlipped();
        return c->Record({{"in", t_arr}, {"out", t_out}});
      });
    Generator* getArrT = mantle->newGeneratorDecl("getArrT", getArrTTypeGen, getArrTParams);

    getArrT->setGeneratorDefFromFun(
      [](Context* c, Values genargs, ModuleDef* def) {
        uint i = genargs.at("i")->get<int>();

        def->connect("self.in." + toString(i), "self.out");
      });
  }

  {
    Params liftArrTParams({{"t", CoreIRType::make(c)}});

    auto liftArrTTypeGen = mantle->newTypeGen(
      "liftArrTTypeFun",
      liftArrTParams,
      [](Context* c, Values args) {
        ArrayType* t_arr = _get_array_type_arg(args, "t");
        int n = t_arr->getLen();
        ASSERT(
          n == 1,
          "Bad lift args! array length must be 1 not " + to_string(n));

        Type* in_type = t_arr->getElemType()->getFlipped();
        return c->Record({{"in", in_type}, {"out", t_arr}});
      });
    Generator* liftArrT = mantle->newGeneratorDecl("liftArrT", liftArrTTypeGen, liftArrTParams);

    liftArrT->setGeneratorDefFromFun(
      [](Context* c, Values genargs, ModuleDef* def) {

        def->connect("self.in", "self.out.0");
      });
  }

  return mantle;
}
