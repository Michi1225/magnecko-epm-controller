LATEX ?= lualatex
BUILD_DIR := build
SOURCE := user-guide.tex
PDF := $(BUILD_DIR)/user-guide.pdf
TEXMFVAR ?= $(abspath $(BUILD_DIR)/texmf-var)
TEXMFCACHE ?= $(TEXMFVAR)
export TEXMFVAR TEXMFCACHE

.PHONY: all clean

all: $(PDF)

$(PDF): $(SOURCE) Makefile
	mkdir -p $(BUILD_DIR) $(TEXMFVAR) $(TEXMFCACHE)
	$(LATEX) -interaction=nonstopmode -halt-on-error -file-line-error -output-directory=$(BUILD_DIR) $(SOURCE)
	$(LATEX) -interaction=nonstopmode -halt-on-error -file-line-error -output-directory=$(BUILD_DIR) $(SOURCE)
	$(LATEX) -interaction=nonstopmode -halt-on-error -file-line-error -output-directory=$(BUILD_DIR) $(SOURCE)

clean:
	rm -rf -- $(BUILD_DIR)
