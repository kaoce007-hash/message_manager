CXX = g++
AR = ar
CPPFLAGS += -Iinclude -Ithird_party
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -std=c++17 -fPIC
LIB_SOURCES := $(wildcard src/*.cpp)
LIB_OBJECTS := $(patsubst src/%.cpp,build/obj/%.o,$(LIB_SOURCES))
LIBRARY := lib/libmessage_manager.a
TEST_NAMES := indexed_set_tests direct_alignment_tests legacy_tests tests alignment_tests msgcode_tests interface_v6_tests interface_v7_tests field_mapping_tests bitfield_tests integer_format_tests simple_structure_tests audit_integration_tests
TEST_BINS := $(addprefix build/test/,$(TEST_NAMES))

.PHONY: all test clean
all: $(LIBRARY) build/demo build/simple_demo

$(LIBRARY): $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $^

build/obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/simple_demo: examples/simple_main.cpp $(LIBRARY)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

build/demo: examples/main.cpp $(LIBRARY)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $@.d $< $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

build/test/%: test/%.cpp $(LIBRARY)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $@.d $< $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BINS)
	@mkdir -p build/test/output
	cd build/test/output && ../legacy_tests
	cd build/test/output && ../tests ../../../test/data/json
	cd build/test/output && ../alignment_tests ../../../test/data/json
	cd build/test/output && ../msgcode_tests ../../../test/data/v5/json
	cd build/test/output && ../interface_v6_tests ../../../test/data/v6/json
	cd build/test/output && ../interface_v7_tests ../../../test/data/current/json
	cd build/test/output && ../field_mapping_tests ../../../json
	cd build/test/output && ../audit_integration_tests ../../../test/data/current/json
	cd build/test/output && ../bitfield_tests ../../../json/bitfields
	cd build/test/output && ../integer_format_tests ../../../json/integer_formats
	cd build/test/output && ../simple_structure_tests ../../../json/integer_formats
	cd build/test/output && ../indexed_set_tests
	cd build/test/output && ../direct_alignment_tests
	python3 -I test/audit_wire_oracle.py build/test/output/audit_output/oracle_cases.json

clean:
	rm -rf build $(LIBRARY)

-include $(LIB_OBJECTS:.o=.d) build/demo.d $(addsuffix .d,$(TEST_BINS))
