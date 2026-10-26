#!/bin/sh
sed -i 's/#include <Time.h>/#include <ctime>/g' Core/*
sed -i 's/#include <time.h>/#include <ctime>/g' Core/*
sed -i 's/#include <Windows.h>/ \/\/ #include <Windows.h>/g' Core/*
sed -i 's/#include <String.h>/#include <cstring>/g' Core/*
sed -i 's/#include <Stdlib.h>/#include <cstdlib>/g' Core/*
sed -i 's/#include <Assert.h>/#include <cassert>/g' Core/*
sed -i 's/#include "Zip.h"/#include "zip.h"/g' Core/* Lib/*
sed -i 's/#include "Unzip.h"/#include "unzip.h"/g' Core/* Lib/*
sed -i 's/#include "Zlib.h"/#include "zlib.h"/g' Core/* Lib/*
