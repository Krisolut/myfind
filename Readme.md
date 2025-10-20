make         # kompiliert myfind
./myfind -R -i ./ test.txt test.doc test

make clean   # löscht myfind und alle .o-Dateien



./myfind -Ri ./ find_1 find_2_1
./myfind -R ./ find_1 find_1_1 find_2_1
./myfind -R ./ Find_1 Find_2_1