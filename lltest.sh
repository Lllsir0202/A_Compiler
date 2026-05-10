#Test llvmir
./bin/compiler test.sy -o test.ll -llvm
clang test.ll -c -o tmp.o -w
clang -static tmp.o -L./libs -l:libx86.a -o test
./test
echo $?
rm -rf tmp.o test