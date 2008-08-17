%define git_repo dahdi-tools
%define git_head xrg

%define name dahdi-tools
%define version 2.0
%define release alpha
#define kernelrelease %(uname -r)

%define	major 2
%define libname	%mklibname tonezone %{major}
%define libnamedevel %mklibname tonezone -d

Summary:	DAHDI userland tools
Name:		dahdi-tools
Version:	%{version}
Release:	%{release}
License:	GPL
Group:		System/Tools
URL:		http://www.asterisk.org/
Source0:	%{name}-%{version}.tar.gz
BuildRequires:	newt-devel
#BuildConflicts:	libtonezone-devel # No longer, DAHDI can co-exist I hope.
BuildRoot:	%{_tmppath}/%{name}-%{version}-root


%description
Asterisk Hardware Device Interface, the userspace tools.
This package includes configuration files, tool binaries etc.

%package -n	%{libname}
Summary:	The shared DAHDI Library
Group:          System/Libraries

%description -n	%{libname}
The shared DAHDI Library

%package -n	%{libnamedevel}
Summary:	Development files for the DAHDI Library
Group:		Development/C
Provides:	tonezone-devel = %{version}-%{release}
Provides:	libtonezone-devel = %{version}-%{release}
Requires:	%{libname} = %{version}-%{release}

%description -n	%{libnamedevel}
Development files for the DAHDI Library.

This package contains the tonezone library and its header
files.

%prep
%git_get_source
%setup -q


%build
pushd trunk
%configure
%make
popd

%install
install -d %{buildroot}%{_sysconfdir}/udev/rules.d
install -d %{buildroot}%{_sysconfdir}/init.d
install -d %{buildroot}/lib/firmware
pushd trunk
%make DESTDIR=%{buildroot} install config
popd

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%attr(0750,root,root)		%{_sbindir}/*
%attr(0644,root,root)		%{_mandir}/man8/*
%attr(0750,root,root)		/usr/share/dahdi/xpp_fxloader
%attr(0644,root,root)		/usr/lib/perl5/site_perl/*/Dahdi.pm
%attr(0644,root,root)		/usr/lib/perl5/site_perl/*/Dahdi/*
%config(noreplace)		/etc/dahdi/init.conf
%config(noreplace)		/etc/dahdi/modules
%config(noreplace)		/etc/dahdi/system.conf
%config				/etc/hotplug/usb/xpp_fxloader
%config				/etc/hotplug/usb/xpp_fxloader.usermap
%config(noreplace)		/etc/modprobe.d/dahdi
%config(noreplace)		/etc/modprobe.d/dahdi.blacklist
%config				/etc/rc.d/init.d/dahdi
%config				/etc/sysconfig/network-scripts/ifup-hdlc
%config				/etc/udev/rules.d/xpp.rules

%files -n %{libname}
%defattr(-,root,root)
%doc trunk/README
%{_libdir}/*.so.*

%files -n %{libnamedevel}
%defattr(-,root,root)
%{_includedir}/dahdi/*.h
%{_libdir}/*.so
%{_libdir}/*.a

