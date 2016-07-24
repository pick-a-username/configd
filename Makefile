TARGET=customsh

DEPDIR = .deps
$(shell mkdir -p $(DEPDIR) >/dev/null)

CXX = g++
CXXFLAGS += -std=c++14
CXXFLAGS += -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
#CXXFLAGS += -Wall -O0 -g
CXXFLAGS += -Wall -O9

LINKER  = g++ -std=c++14 -o
LDFLAGS += -Wall
LDLIBS = -lm -lpthread

MODULES = \
	module_test1.cpp \
	module_test2.cpp \
	module_test3.cpp

SOURCES = \
	ns.cpp \
	customsh.cpp \
	daemonized.cpp \
	$(MODULES) \
	main.cpp


OBJECTS = $(SOURCES:%.cpp=%.o)

all: $(TARGET) customsh-put

$(TARGET): $(OBJECTS)
	@echo "  LD  "$@
	@$(LINKER) $@ $(OBJECTS) $(LDFLAGS) $(LDLIBS)
	@strip $@

customsh-put: customsh-put.o
	@echo "  LD  "$@
	@$(LINKER) $@ customsh-put.o $(LDFLAGS) $(LDLIBS)
	@strip $@

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d
	@echo "  CC  "$<
	@$(CXX) $(CXXFLAGS) -c $<

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	@echo "  CC  "$<
	@$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -rf $(TARGET) customsh-put *.o $(DEPDIR)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SOURCES)))

