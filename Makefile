NAME := webserv

# Directories
OBJDIR := obj
SRCDIR := src
INCDIRS := include

# Default compiler flags
CXX := c++
def_CXXFLAGS := -Wall -Wextra -Werror -g -std=c++17
def_CPPFLAGS := -MMD -MP $(addprefix -I ,$(INCDIRS))

# Project source files
SRCS := $(addprefix $(SRCDIR)/,\
	main.cpp \
)

# Project targets
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
$(NAME): $(OBJS)
BINS := $(NAME)

# Default recipes for each type of target
$(OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(mktargetdir)
	$(CXX) $(_CPPFLAGS) $(_CXXFLAGS) -c $< -o $@

$(BINS):
	$(mktargetdir)
	$(CXX) $(_LDFLAGS) $^ $(_LDLIBS) -o $@

# Utility targets
.DEFAULT_GOAL := all

.PHONY: all clean fclean re

all: $(NAME)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(BINS)

re: fclean all

# Add sanitizer flags if requested (e.g. `make SANITIZE=address,undefined`)
ifneq (,$(strip $(SANITIZE)))
	def_CXXFLAGS += -fsanitize=$(SANITIZE)
	def_LDFLAGS += -fsanitize=$(SANITIZE)
endif

# Combine default def_FLAGS, target specific tgt_FLAGS and user-supplied FLAGS
# into one _FLAGS variable to be used in recipes
flagvars = CXXFLAGS CPPFLAGS LDFLAGS LDLIBS
$(foreach v,$(flagvars),$(eval _$v = $$(strip $$(def_$v) $$(tgt_$v) $$($v))))

# Recipe command to ensure directory for target exists
mktargetdir = @mkdir -p $(@D)

# Don't remake intermediate objects if targets are up-to-date
.SECONDARY: $(OBJS)

# Dependency files to handle #include dependencies
DEPS := $(OBJS:.o=.d)
-include $(DEPS)
