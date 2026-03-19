## HWCDC Arduino bug (18.03.2026)

There is a hard bug causing random kernel panics when using arduino HWCDC over JTAG IDF engine. The solution is to bypass this engine and use IDF JTAG console directly hooked to . Bug is documented in debug/solved/#0001_hwcdc.md. The build selector allready implemented in hx_build.h.