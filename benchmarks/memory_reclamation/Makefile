#
# Copyright (c) 2014-2015, Hewlett-Packard Development Company, LP.
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
# more details. You should have received a copy of the GNU General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# HP designates this particular file as subject to the "Classpath" exception
# as provided by HP in the LICENSE.txt file that accompanied this code.
#
# The "Classpath" exception is restated below:
#
# Certain source files distributed by Hewlett-Packard Company and/or its 
# affiliates(“HP”) are subject to the following clarification and special 
# exception to the GPL, but only where HP has expressly included in the 
# particular source file's header the words "HP designates this particular file 
# as subject to the "Classpath" exception as provided by HP in the LICENSE file 
# that accompanied this code."
# 
# Linking this library statically or dynamically with other modules is making 
# a combined work based on this library.  Thus, the terms and conditions of the 
# GNU General Public License cover the whole combination.
#
# As a special exception, the copyright holders of this library give you 
# permission to link this library with independent modules to produce an 
# executable, regardless of the license terms of these independent modules, 
# and to copy and distribute the resulting executable under terms of your 
# choice, provided that you also meet, for each linked independent module, 
# the terms and conditions of the license of that module. An independent module 
# is a module which is not derived from or based on this library. If you modify 
# this library, you may extend this exception to your version of the library, 
# but you are not obligated to do so. If you do not wish to do so, delete this 
# exception statement from your version.
#



# =========  Make To End All Make  ==============
# 
# A makefile so you don't need to touch a 
# makefile again.  Or at least avoid relearning
# the makefile language.
# ===============================================


# ===============================================
# User configured variables
# ===============================================

# -------------------------
# Paths configuration
# -------------------------

# list of directories which contain headers (include directories)
IDIRS:=./src ./ext/parharness ./src/trackers ../../include
# directory to put build artifacts (e.g. .o, .d files)
ODIR:=./obj
# list of directories which contain linked in libraries
LDIRS:=./ext/parharness
# libraries to link against (are expected to be in the above directories,
# or they are system default)
LIBS :=-ljemalloc -lparharness -lpthread -lhwloc -lm -lrt
# directories that should be built first using recursive make.
# You should avoid this in general, but it's useful for building
# external libraries which we depend on
RECURSEDIRS := ext/parharness 
# root directories of sources.  Will be recursively
# searched for .c and .cpp files.  All of them will
# be built
SRCDIRS :=./src
# output directory for binary executables
BINDIR :=./bin
# C compiler
CC:=gcc
# C++ compiler
CXX:=g++-9
# linker
LD:= $(CXX)


# we input LIBS and output ARCHIVES
# the name of the outputed .a archive file of all non executable source
# name defaults to the name of the Makefile's parent directory
STATICARCHIVE:=
# the name of the outputed .so shared library
# file of all non executable source
# name defaults to the name of the Makefile's parent directory
SHAREDARCHIVE:=

# the directory to output the .a and .so files
# path defaults to BINDIR
ARCHIVEDIR:=./lib


# A list of source files with a "main" method.
# -generated executables have the same name 
# as their source file (without the .c/.cpp suffix)
# -since all executables go into the same folder,
# they must each have unique file names (even if the
# source files are in different folders)
# -since we do pattern matching between this list and the
# source files, the file path specified must be the same
# type (absolute or relative)
EXECUTABLES:= ./src/main.cpp 

# A list of source files contained in the
# source directory to exclude from the build
# -since we do pattern matching between this list and the
# source files, the file path specified must be the same
# type (absolute or relative)
IGNORES:=


# -------------------------
# Flags configuration
# -------------------------

# Default warning flags, only used in CFLAGS,
# and CXXFLAGS
WARNING_FLAGS:=-ftrapv -Wreturn-type -W -Wall \
-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

# Default build flags.   -fsanitize=address,undefined
CFLAGS:=-pthread -g -gdwarf-2 -fpic $(WARNING_FLAGS)
CXXFLAGS:= -pthread -std=gnu++2a -mcx16 -g -fpic -fuse-ld=gold -fconcepts $(WARNING_FLAGS)

# linker flags
LDFLAGS := -fuse-ld=gold $(WARNING_FLAGS)


# -------------------------
# Build configurations
# -------------------------

# A "build" is a separate make target and generates
# its own compilation artifacts.  A build sets
# the $(BUILD) variable when built, so you can
# add build specific logic (see below where we 
# define these build configurations).
# To run a build, e.g. release, you would invoke:
# make release
BUILDS :=release debug ngc release32 debug32
DEFAULT_BUILD :=release

# -------------------------------
# Build and build var definitions
# -------------------------------
ifeq ($(BUILD),release)
CXXFLAGS += -O3 -DNDEBUG -DNO_DESTRUCT
CFLAGS += -O3 -DNDEBUG -DNO_DESTRUCT
# we can add additional release customization here
# e.g. link against different libraries, 
# define enviroment vars, etc.
endif

ifeq ($(BUILD),debug)
CXXFLAGS += -O0 -fsanitize=address,undefined
CFLAGS += -O0 -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
# we can add additional debug customization here
# e.g. link against different libraries, 
# define enviroment vars, etc.
endif

ifeq ($(BUILD),ngc)
CXXFLAGS += -O3 -DNGC
CFLAGS += -O3 -DNGC
endif

ifeq ($(BUILD),release32)
CXXFLAGS += -O3 -m32
CFLAGS += -O3 -m32
LDFLAGS += -m32
# we can add additional release customization here
# e.g. link against different libraries, 
# define enviroment vars, etc.
endif

ifeq ($(BUILD),debug32)
CXXFLAGS += -O0 -m32
CFLAGS += -O0 -m32
LDFLAGS += -m32
# we can add additional release customization here
# e.g. link against different libraries, 
# define enviroment vars, etc.
endif

# Annoying warning flags.
# These are added when environment variable ANNOYING=true
# e.g. make release ANNOYING=true
ANNOYING_FLAGS:= -Wall -W -Wextra -Wundef -Wshadow \
-Wunreachable-code -Wredundant-decls -Wunused-macros \
-Wcast-qual -Wcast-align -Wwrite-strings -Wmissing-field-initializers \
-Wendif-labels -Winit-self -Wlogical-op -Wmissing-declarations \
-Wpacked -Wstack-protector -Wformat=2 -Wswitch-default -Wswitch-enum \
-Wunused -Wstrict-overflow=5 -Wpointer-arith -Wnormalized=nfc \
-Wlong-long -Wconversion -Wmissing-format-attribute -Wpadded \
-Winline -Wvariadic-macros -Wvla -Wdisabled-optimization \
-Woverlength-strings -Waggregate-return -Wmissing-prototypes \
-Wstrict-prototypes -Wold-style-definition -Wbad-function-cast \
-Wc++-compat -Wjump-misses-init -Wnested-externs \
-Wdeclaration-after-statement -ftrapv 

ifeq ($(ANNOYING),true)
CXXFLAGS += $(ANNOYING_FLAGS)
CFLAGS += $(ANNOYING_FLAGS)
endif


# -------------------------------
# Outputted artifacts
# -------------------------------

# what targets get built by default
ARTIFACTS := .info .bin
# .bin : compiles all executables
# .lib : compiles archive files (.a , .so)
# .static: compiles static archive (.a)
# .shared: compiles shared library (.so)
# .info : prints information about compilation variables


# =============================================
# End of user configured variables
# =============================================




# =============================================
# Automatic stuff below. Hopefully you don't 
# need to access below here.  If you do,
# be careful.
# =============================================

# TODOs
# bring in line with http://aegis.sourceforge.net/auug97.pdf
# find executable source?
# comment for non joe users
# fix clean for builds
# faster evaluation? autocache?

# -------------------------
# Utility Functions
# -------------------------

# because make doesn't have a newline character
define \n

endef

# because make lacks equals
eq =$(and $(findstring $(1),$(2)),$(findstring $(2),$(1)))

# -------------------------
# Generate static variables
# -------------------------

_HOSTNAME:=$(shell hostname)
MKFILE_PATH :=$(abspath $(lastword $(MAKEFILE_LIST)))
DIR_PATH :=$(patsubst %/,%,$(dir $(MKFILE_PATH)))
CURRENT_DIR :=$(notdir $(patsubst %/,%,$(dir $(MKFILE_PATH))))

_DEPDIRS:=$(IDIRS)
_IDIRS:=$(foreach d, $(IDIRS), -I $d)
_LDIRS:=$(foreach d, $(LDIRS), -L $d)
_C_COMP_ARGS:= $(CFLAGS) $(_IDIRS)
_CXX_COMP_ARGS:= $(CXXFLAGS) $(_IDIRS)
_LINK_ARGS:= $(LIBS) $(_LDIRS) $(LDFLAGS)


_IGNORES_FULL_PATHS:=$(foreach x, $(IGNORES), $(patsubst ./%,%,$(x)))
_EXECUTABLE_FULL_PATHS:=$(foreach x, $(EXECUTABLES), $(patsubst ./%,%,$(x)))

_DEPS :=$(MKFILE_PATH)
_SRCS_C :=$(foreach d, $(SRCDIRS), $(shell find $d/ -iname '*.c' -type f | sed 's/ /\\ /g'))
_SRCS_CPP :=$(foreach d, $(SRCDIRS), $(shell find $d/ -iname '*.cpp' -type f | sed 's/ /\\ /g'))
_SRCS := $(foreach x, $(_SRCS_C) $(_SRCS_CPP), $(patsubst ./%,%,$(x)))
_SRCS_FULL_PATHS :=$(filter-out $(_EXECUTABLE_FULL_PATHS) $(_IGNORES_FULL_PATHS), $(foreach x, $(_SRCS), $(x)))

VPATH+=:$(IDIRS)
 
# -------------------------
# Build configuration rules
# -------------------------


ifneq ($(BUILD),)
_BUILD_THIS:=$(BUILD)
else
_BUILD_THIS:=$(DEFAULT_BUILD)
endif

ifeq ($(strip $(ARCHIVEDIR)),)
	ARCHIVEDIR:=$(BINDIR)
endif

_BUILD_DIR:=$(patsubst ./%,%,$(patsubst %/,%,$(strip $(ODIR))))
_BUILD:=$(patsubst ./%,%,$(patsubst %/,%,$(strip $(_BUILD_THIS))))
_ODIR:=$(_BUILD_DIR)/$(_BUILD)
_BINDIR:=$(patsubst ./%,%,$(patsubst %/,%,$(strip $(BINDIR))))/$(_BUILD)
_ARCHIVEDIR:=$(patsubst ./%,%,$(patsubst %/,%,$(strip $(ARCHIVEDIR))))/$(_BUILD)

ifeq ($(strip $(STATICARCHIVE)),)
	_STATICARCHIVE:=$(_ARCHIVEDIR)/lib$(CURRENT_DIR).a
else
	_STATICARCHIVE:=$(_ARCHIVEDIR)/$(STATICARCHIVE)
endif

ifeq ($(strip $(SHAREDARCHIVE)),)
	_SHAREDARCHIVE:=$(_ARCHIVEDIR)/lib$(CURRENT_DIR).so
else
	_SHAREDARCHIVE:=$(_ARCHIVEDIR)/$(SHAREDARCHIVE)
endif


morph=$(_ODIR)/$(basename $(strip $(1)))
unmorph=$(patsubst $(_ODIR)/%, %, $(basename $(strip $(1))))
_OBJECTS_MOVED :=$(foreach x, $(_SRCS_FULL_PATHS), $(call morph,$(x)).o)
_EXECUTABLE_OBJECTS_MOVED :=$(foreach x, $(_EXECUTABLE_FULL_PATHS), $(call morph,$(x)).o)
_BINS_MOVED :=$(foreach x,$(_EXECUTABLE_FULL_PATHS),$(_BINDIR)/$(notdir $(basename $(x))))
_BUILD_SUBDIRS :=$(foreach x, $(_OBJECTS_MOVED) $(_EXECUTABLE_OBJECTS_MOVED), $(dir $(x)))
find_morphed_executable=$(filter $(_ODIR)/$(notdir $(basename $1)).o %/$(notdir $(basename $1)).o, $(_EXECUTABLE_OBJECTS_MOVED))


# we use some serious voodoo here.
# $(-*-command-variables-*-) gives us the command line arguments.  It's undocumented.
# http://stackoverflow.com/questions/23919199/detecting-unused-makefile-command-line-arguments
default:
	for rec in $(RECURSEDIRS); do $(MAKE) -C $$rec; done
	$(MAKE) .artifacts -f $(MKFILE_PATH) BUILD=$(DEFAULT_BUILD) $(-*-command-variables-*-)

$(BUILDS): 
	for rec in $(RECURSEDIRS); do $(MAKE) -C $$rec; done
	$(MAKE) .artifacts -f $(MKFILE_PATH) BUILD=$@ $(-*-command-variables-*-)

.PHONY: .artifacts
.artifacts: $(ARTIFACTS)
	cp $(_BINS_MOVED) $(dir $(_BINDIR)) 2>/dev/null || :
	cp $(_STATICARCHIVE) $(dir $(_ARCHIVEDIR)) 2>/dev/null || :
	cp $(_SHAREDARCHIVE) $(dir $(_ARCHIVEDIR)) 2>/dev/null || :

# -------------------------
# Print variables
# -------------------------
.info:
	$(info ${\n})
	$(info Makefile - $(strip ${MKFILE_PATH}))
	$(info Include Dirs - ${IDIRS})
	$(info Lib Dirs - ${LDIRS})
	$(info Executable Source Files - ${_EXECUTABLE_FULL_PATHS})
	$(info Nonexecutable Source Files - ${_SRCS_FULL_PATHS})
	$(info Targets - ${_BINS_MOVED})
	$(info Nonexecutable Objects - ${_OBJECTS_MOVED})
	$(info Executable Objects - ${_EXECUTABLE_OBJECTS_MOVED})
	$(info Static Archive File - ${_STATICARCHIVE})
	$(info Shared Object File - ${_SHAREDARCHIVE})
	$(info Path - ${DIR_PATH})
	$(info Host - ${_HOSTNAME})
	$(info ${\n})

# ========================
# Actual Build Rules
# ========================
# Building can only happen at the second
# level of recursion once we've set up the build
# We elide the actual rules to make -pq faster
ifneq (0,${MAKELEVEL})


# ------------------------
# Makefile pragmas
# ------------------------

# for sorcery.  just look it up in the manual.
.SECONDEXPANSION: 
# for getting rid of implicit rules, because they make debugging hard
.SUFFIXES: 


# ------------------------
# Create directories
# ------------------------
create_dirs:=$(foreach d, $(_ODIR) $(_BINDIR) $(_ARCHIVEDIR) $(_BUILD_SUBDIRS),$(shell test -d $(d) || mkdir -p $(d)))
create_build_dir:=$(shell test -d $(_BUILD_DIR) || mkdir -p $(_BUILD_DIR)))

# ------------------------
# Build rules for target
# ------------------------

.bin: $(_BINS_MOVED)

$(_BINS_MOVED): $$(call find_morphed_executable,$$@) $(_OBJECTS_MOVED) $(_DEPS)
	$(if $(findstring .atl,$@), \
		$(ALD) -o $@ $(_OBJECTS_MOVED) $(call find_morphed_executable,$@) -latlas  $(_LINK_ARGS) ,  \
		$(LD) -o $@ $(_OBJECTS_MOVED) $(call find_morphed_executable,$@) $(_LINK_ARGS) ) 

# ---------------------
# Library build rules
# ---------------------

# builds a libraries of all 
# source (minus executables)
.lib: .static .shared
.static: $(_STATICARCHIVE)
.shared: $(_SHAREDARCHIVE)

$(_STATICARCHIVE): $(_OBJECTS_MOVED) $(_DEPS)
	$(if $(_OBJECTS_MOVED), ar rcs $(_STATICARCHIVE) $(_OBJECTS_MOVED) , )
	touch $(_STATICARCHIVE)

$(_SHAREDARCHIVE): $(_OBJECTS_MOVED) $(_DEPS)
	$(if $(_OBJECTS_MOVED), gcc -shared -o $(_SHAREDARCHIVE) $(_OBJECTS_MOVED) , )

# ------------------------------
# Dependency handling
# ------------------------------

-include $(_OBJECTS_MOVED:.o=.d)
-include $(_EXECUTABLE_OBJECTS_MOVED:.o=.d)


%.d: $$(call unmorph, $$@).c $(_DEPS)
	$(CC) -MM $(call unmorph, $@).c $(_C_COMP_ARGS) | sed -e "s?$(notdir $*).o:?$*.d:?"  > $@

%.d: $$(call unmorph, $$@).cpp $(_DEPS)
	$(CXX) -MM $(call unmorph, $@).cpp $(_CXX_COMP_ARGS) | sed -e "s?$(notdir $*).o:?$*.d:?"  > $@

# -------------------------
# atlas specific handling
# -------------------------

# Compile c code
%.atl.o: $$(call unmorph, $$@).c $(_DEPS) %.d
	$(ACC) -c  $(call unmorph, $@).c -o $@ $(_C_COMP_ARGS)

# Compile cpp code
%.atl.o: $$(call unmorph, $$@).cpp $(_DEPS) %.d
	$(ACXX) -c $(call unmorph, $@).cpp -o $@ $(_CXX_COMP_ARGS) 

# ------------------------------
# Default build rules for c/c++
# ------------------------------

# Compile c code
%.o: $$(call unmorph, $$@).c $(_DEPS) %.d
	$(CC) -c $(call unmorph, $@).c -o $@ $(_C_COMP_ARGS) 

# Compile cpp code
%.o: $$(call unmorph, $$@).cpp $(_DEPS) %.d
	$(CXX) -c $(call unmorph, $@).cpp -o $@ $(_CXX_COMP_ARGS) 

# ========================
# End of Actual Build Rules
# ========================
endif 


# ---------------------
# Auxiliary build rules
# ---------------------

clean:
	rm Makefile~ 2> /dev/null || :
	rm -r $(_BUILD_DIR) *.o *.d 2> /dev/null || :
	$(foreach d, $(SRCDIRS), $(shell rm $d/*.c~ $d/*.cpp~ $d/*.h~ $d/*.hpp~ 2> /dev/null || :) )
	$(foreach d, $(SRCDIRS), $(info rm ${d}/*.c~ ${d}/*.cpp~ ${d}/*.h~ ${d}/*.hpp~ 2> /dev/null || :) )
	$(foreach d, $(BUILDS) . , $(info rm $(dir $(_ARCHIVEDIR))$d/$(notdir $(_STATICARCHIVE)) $(dir $(_ARCHIVEDIR))$d/$(notdir $(_SHAREDARCHIVE)) 2> /dev/null || : ) )
	$(foreach d, $(BUILDS) . , $(shell rm $(dir $(_ARCHIVEDIR))$d/$(notdir $(_STATICARCHIVE)) $(dir $(_ARCHIVEDIR))$d/$(notdir $(_SHAREDARCHIVE)) 2> /dev/null || : ) )
	$(foreach d, $(BUILDS) . , $(foreach b, $(notdir $(_BINS_MOVED)), $(info rm $(dir $(_BINDIR))$d/$b 2> /dev/null || : )) )
	$(foreach d, $(BUILDS) . , $(foreach b, $(notdir $(_BINS_MOVED)), $(shell rm $(dir $(_BINDIR))$d/$b 2> /dev/null || : )) )


clean_memory:
	rm -f -r /dev/shm/* || :

print_%: ; @echo $*=$($*)




