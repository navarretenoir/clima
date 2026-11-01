# Clima (C) — GTK4 + libadwaita + WebKitGTK 6 + libcurl + json-glib
# La raíz del proyecto se incrusta para hallar ui/ y data/ en tiempo de ejecución.
ROOT   := $(abspath $(CURDIR)/..)
# La versión es el archivo VERSION de la raíz: una sola fuente de verdad.
VERSION ?= $(shell cat $(ROOT)/VERSION 2>/dev/null || cat ../VERSION 2>/dev/null || echo 0.0)
PKGS   := gtk4 libadwaita-1 webkitgtk-6.0 json-glib-1.0 libcurl libgeoclue-2.0
CFLAGS := -O2 -Wall -Wextra -std=c11 $(shell pkg-config --cflags $(PKGS)) \
          -DCLIMA_ROOT='"$(ROOT)"' -DCLIMA_VERSION='"$(VERSION)"' -D_GNU_SOURCE $(EXTRA_CFLAGS)
LIBS   := $(shell pkg-config --libs $(PKGS)) -lm
SRC    := clima.c clima_data.c clima_cli.c
BIN    := clima
SCRAPERS := suspension-scraper restriccion-scraper

all: $(BIN) $(SCRAPERS)

$(BIN): $(SRC) clima_data.h clima_ambiental_c.inc
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LIBS)

# Módulos scraper — C++
suspension-scraper: suspension_scraper.cpp
	$(CXX) -O2 -std=c++17 suspension_scraper.cpp $(shell pkg-config --libs libcurl) -o $@

restriccion-scraper: restriccion_scraper.cpp
	$(CXX) -O2 -std=c++17 restriccion_scraper.cpp $(shell pkg-config --libs libcurl) -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: run clean
