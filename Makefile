# --------------- DEFS --------------- #

NAME = ft_nm

all: $(NAME)

# --------------- NASM --------------- #

NASM = nasm
NASM_IFLAGS = -I ./inc/asm
NASM_CFLAGS =

ifeq ($(OS),Windows_NT)
	NASM_CFLAGS += -f win64
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		NASM_CFLAGS += -f elf64
	endif
	ifeq ($(UNAME_S),Darwin)
		NASM_CFLAGS += -f macho64
	endif
endif

# --------------- $(CC) --------------- #

MAKE_FILES =
CFLAGS = -O2 -g
IFLAGS = -I $(INC_DIR)
LFLAGS = -lm
DEPS =

# --------------- Libraries --------------- #

LIBFT_NAME = ft
LIBFT_DIR = libft
LIBFT_A = $(LIBFT_DIR)/lib$(LIBFT_NAME).a

MAKE_FILES += $(LIBFT_DIR)
IFLAGS += -I $(LIBFT_DIR)/inc
LFLAGS += -L $(LIBFT_DIR) -l$(LIBFT_NAME)
DEPS += $(LIBFT_A)

$(LIBFT_A):
	$(MAKE) -C $(LIBFT_DIR)

# --------------- LS --------------- #

SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj

SRC_FILES = $(shell find $(SRC_DIR) -type f -regex '.*.c$$' 2> /dev/null)
SRC_FILES += $(shell find $(SRC_DIR) -type f -regex '.*.asm$$' 2> /dev/null)
INC_FILES = $(shell find $(INC_DIR) -type f -regex '.*\.h$$' 2> /dev/null)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.o, $(SRC_FILES)))

$(NAME): $(OBJ_FILES) $(DEPS)
	$(CC) -o $@ $(OBJ_FILES) $(CFLAGS) $(LFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_FILES)
	@mkdir -p $$(dirname $@)
	$(CC) -o $@ -c $< $(CFLAGS) $(IFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	$(NASM) $(NASM_IFLAGS) $(NASM_CFLAGS) -o $@ $<

clean:
	@$(foreach var,$(MAKE_FILES),$(MAKE) -C $(var) clean;)
	@rm -rf $(OBJ_DIR)

fclean: clean
	@$(foreach var,$(MAKE_FILES),$(MAKE) -C $(var) fclean;)
	@rm -f $(NAME)

re:
	@$(MAKE) fclean
	@$(MAKE)
