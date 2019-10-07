# GNUmakefile
#       (c) S.Suzuki 2017.7.21 -> modified 2019.10.07 T.Suehara

SUFFIX   = .cc
SUFFIXH  = .h
COMPILER = g++
CFLAGS   = -O2
LFLAGS   = -lpthread -lstdc++ -L./lib -lftd2xx -llalusb20

SRCDIR   = ./src
INCLUDE  = ./include
OBJDIR   = ./obj
EXEDIR   = ./bin

SOURCES  = $(wildcard $(SRCDIR)/*$(SUFFIX))
INCLUDES = $(wildcard $(INCLUDE)/*$(SUFFIXH))
OBJECTS  = $(notdir $(SOURCES:$(SUFFIX)=.o))
TARGETS  = $(notdir $(basename $(SOURCES)))

define MAKEALL
$(1): $(EXEDIR)/$(1)
	
$(EXEDIR)/$(1): $(OBJDIR)/$(1).o
	$(COMPILER) $(LFLAGS) -o $(EXEDIR)/$(1) $(OBJDIR)/$(1).o
$(OBJDIR)/$(1).o: $(SRCDIR)/$(1)$(SUFFIX) $(INCLUDES)
	$(COMPILER) -I$(INCLUDE) $(CFLAGS) -o $(OBJDIR)/$(1).o -c $(SRCDIR)/$(1)$(SUFFIX)
endef

.PHONY: all
all: $(TARGETS)
$(foreach VAR,$(TARGETS),$(eval $(call MAKEALL,$(VAR))))

#make clean
.PHONY: clean
clean: 
	$(RM) $(EXEDIR)/* 
	$(RM) $(OBJDIR)/* 

