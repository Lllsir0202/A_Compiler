inline auto IsLibraryFunction(const std::string &func_name) -> bool {
  static std::unordered_set<std::string> library_functions = {"getint",    "getch",     "getfloat",  "getarray",
                                                              "getfarray", "putint",    "putch",     "putfloat",
                                                              "putarray",  "putfarray", "_sysy_starttime", "_sysy_stoptime"};
  return library_functions.count(func_name) > 0;
}