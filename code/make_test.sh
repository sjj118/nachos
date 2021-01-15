cd ~
cp -r nachos-3.4 temp
cd temp/code/test
rm *.o
make
cd ~
rm -r nachos-3.4/code/test
mv temp/code/test nachos-3.4/code/
rm -r temp
cd nachos-3.4/code/
