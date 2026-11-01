Name:           clima
Version:        1.2.1
Release:        1%{?dist}
Summary:        Pronóstico para las 346 comunas de Chile, con cielo animado
License:        GPL-3.0-or-later
URL:            https://github.com/navarretenoir/clima
Source0:        https://github.com/navarretenoir/clima/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
BuildRequires:  gcc gcc-c++ make pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(webkitgtk-6.0) pkgconfig(json-glib-1.0) pkgconfig(libcurl) pkgconfig(libgeoclue-2.0)
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

%description
Current weather, 24 hours and 10 days for the 346 communes of Chile, with an animated sky,
air-quality alerts (PM2.5), vehicle restriction, school-suspension risk, moon phase and tides.
Data from Open-Meteo. Includes the clima command-line tool. Documentation and interface in Spanish.

%prep
%autosetup -n clima-%{version}

%build
make -C c ROOT=%{_datadir}/clima VERSION="$(cat VERSION)" EXTRA_CFLAGS="%{optflags} %{build_ldflags} -DCLIMA_LIBEXEC='\"%{_libexecdir}/clima\"'"

%install
D=%{buildroot}
install -Dm755 c/clima "%{buildroot}%{_prefix}/bin/clima"
install -Dm755 c/suspension-scraper "%{buildroot}%{_prefix}/libexec/clima/suspension-scraper"
install -Dm755 c/restriccion-scraper "%{buildroot}%{_prefix}/libexec/clima/restriccion-scraper"
install -d "%{buildroot}%{_prefix}/share/clima"; cp -r ui data "%{buildroot}%{_prefix}/share/clima/"
printf '%%s\n' '#!/bin/sh' ': "${CLIMA_HWACCEL:=never}"; export CLIMA_HWACCEL' 'exec %{_prefix}/bin/clima --gui "$@"' > "%{buildroot}%{_prefix}/bin/clima-app"; chmod 755 "%{buildroot}%{_prefix}/bin/clima-app"
install -Dm644 linux/io.github.navarretenoir.Clima.desktop "%{buildroot}%{_prefix}/share/applications/io.github.navarretenoir.Clima.desktop"
install -Dm644 linux/io.github.navarretenoir.Clima.metainfo.xml "%{buildroot}%{_prefix}/share/metainfo/io.github.navarretenoir.Clima.metainfo.xml"
install -Dm644 linux/io.github.navarretenoir.Clima.svg "%{buildroot}%{_prefix}/share/icons/hicolor/scalable/apps/io.github.navarretenoir.Clima.svg"

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.navarretenoir.Clima.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/io.github.navarretenoir.Clima.metainfo.xml || :

%files
%license LICENSE
%doc README.md
%{_bindir}/*
%{_datadir}/clima
%{_datadir}/applications/io.github.navarretenoir.Clima.desktop
%{_datadir}/metainfo/io.github.navarretenoir.Clima.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.navarretenoir.Clima.svg
%{_libexecdir}/clima

%changelog
* Sun Nov 01 2026 Javiera Navarrete <navarrete.noir@gmail.com> - 1.2.1-1
- Paquete inicial desde el código fuente.
