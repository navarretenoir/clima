# Clima para Windows — compilación cruzada con MinGW-w64 (contenedor «empaquetar», Debian 13).
# Clima.exe   : 32 bits UNICODE, Windows 2000 → 11 (en 64 bits corre por WoW64)
# Clima9x.exe : 32 bits ANSI,   Windows 95/98/Me
# Desarrollado por Javiera Navarrete.
CC      := i686-w64-mingw32-gcc
WINDRES := i686-w64-mingw32-windres
SRC     := src/clima.c src/json.c
COMUN   := -Os -std=gnu89 -Wall -Wno-unused-parameter -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections
LDCOMUN := -mwindows -s -static-libgcc -Wl,--gc-sections -Wl,--nxcompat -Wl,--dynamicbase -Wl,--no-seh -lwininet -lgdi32 -luser32 -lkernel32 -ladvapi32

all: dist/Clima.exe dist/Clima9x.exe

build/clima-nt.o: src/clima.rc src/clima.ico src/comunas.txt src/clima.manifest
	mkdir -p build; $(WINDRES) -DUNICODE -Isrc $< -O coff -o $@
build/clima-9x.o: src/clima.rc src/clima.ico src/comunas.txt
	mkdir -p build; $(WINDRES) -Isrc $< -O coff -o $@

dist/Clima.exe: $(SRC) build/clima-nt.o
	mkdir -p dist
	$(CC) $(COMUN) -DUNICODE -D_UNICODE -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -o $@ $(SRC) build/clima-nt.o $(LDCOMUN) \
	  -Wl,--major-subsystem-version=5,--minor-subsystem-version=0,--major-os-version=5,--minor-os-version=0 -lm

dist/Clima9x.exe: $(SRC) build/clima-9x.o
	mkdir -p dist
	$(CC) $(COMUN) -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -o $@ $(SRC) build/clima-9x.o $(LDCOMUN) \
	  -Wl,--major-subsystem-version=4,--minor-subsystem-version=0,--major-os-version=4,--minor-os-version=0 -lm

clean:
	rm -rf build dist
