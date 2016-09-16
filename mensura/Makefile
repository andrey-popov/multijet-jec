# Make sure mensura is available
ifeq ($(MENSURA_INSTALL), )
  $(error Mandatory environment variable MENSURA_INSTALL is not set)
endif

# Make sure ROOT is available
ifeq ($(shell which root-config), )
  $(error ROOT installation is not found)
endif

# Make sure Boost is available
ifeq ($(BOOST_ROOT), )
  $(error Mandatory environment variable BOOST_ROOT is not set)
endif


# Use bash in $shell command
SHELL := /bin/bash


# Flags to control compilation and linking
INCLUDE := -Iplugins -I$(MENSURA_INSTALL)/include -I$(shell root-config --incdir)
OPFLAGS := -O2
CFLAGS := -Wall -Wextra -std=c++14 $(INCLUDE) $(OPFLAGS)
LDFLAGS := $(shell root-config --libs) -lTreePlayer -lHistPainter \
 -L$(MENSURA_INSTALL)/lib -lmensura -lPECReader -Wl,-rpath=$(MENSURA_INSTALL)/lib \
 -L$(BOOST_ROOT)/lib -lboost_filesystem -Wl,-rpath=$(BOOST_ROOT)/lib


# Programs to be built
PROGS := multijet


# Location of various files of the project
BIN_DIR := bin
SOURCE_DIR := src
PLUGIN_DIR := plugins
OBJ_DIR := obj
OBJECTS := \
	$(shell find ./ -regextype posix-extended -regex "\./$(PLUGIN_DIR)/.*\.cpp" \
	-printf "%f\n" | while read f; do echo "$(OBJ_DIR)/$${f%.cpp}.o"; done)

vpath %.cpp $(SOURCE_DIR) $(PLUGIN_DIR)
vpath %.o $(OBJ_DIR)


# Targets
progs: $(PROGS)
.PHONY: clean


$(PROGS): % : $(OBJECTS) $(OBJ_DIR)/%.o
	mkdir -p $(BIN_DIR)
	g++ $^ $(CFLAGS) $(LDFLAGS) -o $(BIN_DIR)/$@


$(OBJ_DIR)/%.o: %.cpp
	mkdir -p $(OBJ_DIR)
	g++ -c $^ $(CFLAGS) -o $@


clean:
	rm -rf $(OBJ_DIR)
