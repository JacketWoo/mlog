CXX = g++
CXXFLAGS = -Wall -W -Wno-unused-parameter -pg -g -O3 -D__STDC_FORMAT_MACROS -fPIC -std=c++11

OBJECT = mlog
OUTPUT = ./output
INCLUDE_PATH = -I./include
LIBS = -lpthread

LIBRARY = libmlog.a

.PHONY: all clean


BASE_OBJS := $(wildcard src/*.cc)
BASE_OBJS += $(wildcard src/*.c)
BASE_OBJS += $(wildcard src/*.cpp)
OBJS = $(patsubst %.cc,%.o,$(BASE_OBJS))

all: $(LIBRARY)
	@echo "Success, go, go, go..."


$(LIBRARY): $(OBJS)
	rm -rf $(OUTPUT)
	mkdir $(OUTPUT)
	mkdir $(OUTPUT)/include
	mkdir $(OUTPUT)/lib
	rm -rf $@
	ar -rcs $@ $(OBJS)
	cp -rf ./include/*.h $(OUTPUT)/include
	mv ./libmlog.a $(OUTPUT)/lib/
	make -C ./example

$(OBJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(OBJS): %.o : %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@ 

$(TOBJS): %.o : %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< $(INCLUDE_PATH) -o $@ 

clean: 
	rm -rf ./*.o
	rm -rf $(OUTPUT)
	rm -rf $(LIBRARY)
	rm -rf $(OBJECT)
	make clean -C ./example

