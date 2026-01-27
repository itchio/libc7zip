# Convenience Makefile for libc7zip
# Wraps the Node.js build scripts for easier local development

# Auto-detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    OS := linux
endif
ifeq ($(UNAME_S),Darwin)
    OS := darwin
endif

# Auto-detect architecture
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    ARCH := amd64
endif
ifeq ($(UNAME_M),aarch64)
    ARCH := arm64
endif
ifeq ($(UNAME_M),arm64)
    ARCH := arm64
endif

# Allow overrides: make build OS=linux ARCH=arm64
OS ?= linux
ARCH ?= amd64

.PHONY: build test clean help

help:
	@echo "libc7zip build targets:"
	@echo "  make build    - Build libc7zip and p7zip (OS=$(OS) ARCH=$(ARCH))"
	@echo "  make test     - Run test suite (must run build first)"
	@echo "  make clean    - Remove build artifacts"
	@echo ""
	@echo "Override OS/ARCH: make build OS=darwin ARCH=arm64"

build:
	node release/ci-compile.js $(OS) $(ARCH)

test:
	node release/ci-test.js $(OS) $(ARCH)

clean:
	rm -rf build broth source source.tar.bz2 sha1.txt sha256.txt
