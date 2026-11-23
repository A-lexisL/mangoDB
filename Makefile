CXX = $(or $(shell command -v clang++-22 2>/dev/null),$(shell command -v clang++ 2>/dev/null),clang++)
CLANG_FORMAT = $(or $(shell command -v clang-format-22 2>/dev/null),$(shell command -v clang-format 2>/dev/null),clang-format)
CLANG_TIDY = $(or $(shell command -v clang-tidy-22 2>/dev/null),$(shell command -v clang-tidy 2>/dev/null),clang-tidy)
CXXFLAGS = -std=c++20 -Werror -Wall -Wextra -Wconversion -Wvla \
    -Wno-unused-result -Wpedantic -pthread -O2
LDFLAGS = -pthread

TARGET = mangoDB
SRCDIR = src
BUILDDIR = build

SOURCES = $(shell find $(SRCDIR) -type f -name '*.cpp')
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

.PHONY: all clean debug check format

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

debug: CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O0 -g
debug: clean all

check format: private CHECK_FILES = $(shell find $(SRCDIR) -type f \( -name '*.cpp' -o -name '*.h' \))

format:
	@test -n "$(CHECK_FILES)" || { echo "No C++ files found in $(SRCDIR)" >&2; exit 1; }
	$(CLANG_FORMAT) --style=file -i $(CHECK_FILES)

check:
	@test -n "$(SOURCES)" || { echo "No C++ source files found in $(SRCDIR)" >&2; exit 1; }
	$(CLANG_FORMAT) --style=file --dry-run --Werror $(CHECK_FILES)
	$(CLANG_TIDY) $(SOURCES) --config-file=.clang-tidy -- $(CPPFLAGS) $(CXXFLAGS)
	cppcheck --enable=warning,style,performance,portability \
		--std=c++20 --language=c++ --error-exitcode=1 \
		-I$(SRCDIR) $(SOURCES)
	cpplint --repository=. $(CHECK_FILES)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
