# Security policy

## Reporting a vulnerability

Please do not report a vulnerability in a public issue, pull request or discussion: that publishes
it before a fix exists.

Report it privately through GitHub instead: on the repository's **Security** tab, choose
**Report a vulnerability**. Only the maintainers see the report. Please include the version
(`hitokoto --version`), the operating system, how you installed hitokoto (Debian package, AUR,
Windows installer or your own build), and the steps or the file that show the problem.

The maintainers answer in the report and publish the fix as a new release.

## Supported versions

Fixes go into the newest release only; older versions get no separate fixes. Each fix comes as a
new release, with the Debian package and the Windows installer on the
[releases page](https://github.com/Daniel-Fuerst/hitokoto/releases) and the package in the AUR.

| Version | Gets fixes |
| --- | --- |
| newest release | yes |
| older releases | no, update to the newest |

A vulnerability in Qt itself belongs to the Qt project. The Windows installer ships Qt and takes a
fixed Qt with its next release; the Debian and AUR packages use the distribution's Qt and get its
fixes from the distribution.
