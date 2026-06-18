Name:           velora-shell
Version:        2.0.35
Release:        1%{?dist}
Summary:        Lingmo OS Desktop Shell (DDE Shell)
License:        GPL-3.0-or-later
URL:            https://github.com/LingmoOS/velora-shell

# Local source tarball (generated from checkout in CI)
Source0:        velora-shell-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.16
BuildRequires:  extra-cmake-modules
BuildRequires:  pkgconfig(Qt6Core)
BuildRequires:  pkgconfig(Qt6Gui)
BuildRequires:  pkgconfig(Qt6Concurrent)
BuildRequires:  pkgconfig(Qt6Quick)
BuildRequires:  pkgconfig(Qt6QuickTemplates2)
BuildRequires:  pkgconfig(Qt6WaylandClient)
BuildRequires:  pkgconfig(Qt6WaylandCompositor)
BuildRequires:  pkgconfig(Qt6DBus)
BuildRequires:  pkgconfig(Qt6LinguistTools)
BuildRequires:  pkgconfig(Qt6Sql)
BuildRequires:  pkgconfig(dtk6core)
BuildRequires:  pkgconfig(dtk6gui)
BuildRequires:  pkgconfig(icu-uc)
BuildRequires:  pkgconfig(wayland-protocols)
BuildRequires:  pkgconfig(wayland-client)

%description
Lingmo OS Desktop Shell (DDE Shell) is the core shell component
for the Lingmo desktop environment, providing dock, panels,
applets, and desktop management.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DBUILD_WITH_X11=ON \
    -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%files
%doc README.md
%license LICENSE*
%{_libdir}/dde-shell/
%{_libdir}/qt6/qml/org/deepin/ds/
%{_datadir}/dde-shell/
%{_datadir}/dbus-1/services/*.service
%{_datadir}/dbus-1/system-services/*.service
%{_datadir}/dsg/
%{_bindir}/*
%{_libexecdir}/dde-shell/

%changelog
* Tue Jun 18 2025 LingmoOS Build System <dev@lingmo.os> - %{version}-1
- Initial RPM packaging for local source build
