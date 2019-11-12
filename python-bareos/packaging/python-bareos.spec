#
# python-bareos spec file.
#

# based on
# https://docs.fedoraproject.org/en-US/packaging-guidelines/Python_Appendix/
# specifically on
# https://pagure.io/packaging-committee/blob/ae14fdb50cc6665a94bc32f7d984906ce1eece45/f/guidelines/modules/ROOT/pages/Python_Appendix.adoc
#

%global srcname bareos

Name:           python-%{srcname}
Version:        0
Release:        1%{?dist}
License:        AGPL-3.0
Summary:        Backup Archiving REcovery Open Sourced - Python module
Url:            https://github.com/bareos/python-bareos/
Group:          Productivity/Archiving/Backup
Vendor:         The Bareos Team
Source:         %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-root

BuildArch:      noarch

%global _description %{expand:
Bareos - Backup Archiving Recovery Open Sourced - Python module

This packages contains a python module to interact with a Bareos backup system.
It also includes some tools based on this module.}

%description %_description

%package -n python2-%{srcname}
Summary:        %{summary}
BuildRequires:  python2-devel
BuildRequires:  python2-setuptools
%{?python_provide:%python_provide python2-%{srcname}}

%description -n python2-%{srcname} %_description


%package -n python3-%{srcname}
Summary:        %{summary}
BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
%{?python_provide:%python_provide python3-%{srcname}}

%description -n python3-%{srcname} %_description


%prep
#%%autosetup -n %%{srcname}-%%{version}
%setup -q

%build
%py2_build
%py3_build

%install
# Must do the python2 install first because the scripts in /usr/bin are
# overwritten with every setup.py install, and in general we want the
# python3 version to be the default.
%py2_install
%py3_install

%check
# This does not work,
# as "test" tries to download other packages from pip.
#%%{__python2} setup.py test
#%%{__python3} setup.py test

# Note that there is no %%files section for the unversioned python module if we are building for several python runtimes
%files -n python2-%{srcname}
%defattr(-,root,root,-)
%doc README.rst
%{python2_sitelib}/%{srcname}/
%{python2_sitelib}/python_%{srcname}-*.egg-info/

%files -n python3-%{srcname}
%defattr(-,root,root,-)
%doc README.rst
%{python3_sitelib}/%{srcname}/
%{python3_sitelib}/python_%{srcname}-*.egg-info/
%{_bindir}/*

%changelog
