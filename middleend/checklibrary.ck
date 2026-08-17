inline auto IsLibraryFunction(const std::string &func_name) -> bool {
  static std::unordered_set<std::string> library_functions = {"getint",    "getch",     "getfloat",  "getarray",
                                                              "getfarray", "putint",    "putch",     "putfloat",
                                                              "putarray",  "putfarray", "_sysy_starttime", "_sysy_stoptime"};
  // RVV vector intrinsics (`rvv.<op>.<elem>`) are library-declared: they carry a
  // whole vectorizable loop down to the backend, which expands them inline.
  return library_functions.count(func_name) > 0 || func_name.rfind("rvv.", 0) == 0;
}