/////////////////////////////////////////////////////////////////////////////
// The rules that complete an application's adios_config.h.
//
// Evaluated by TWO consumers over the SAME file, so they can never drift:
//   - the C build: adios.h includes this header (which pulls the app's
//     adios_config.h first), so every rule below is already applied when
//     the OS sources read a configuration switch;
//   - the Makefile: include/makefile/app_config.mk runs the preprocessor
//     over this very header and reads the RESULT - which is what lets an
//     application keep even derived values (#if chains) in its config
//     instead of duplicating the derivation in make.
//
// HOW TO USE IT
//   Nothing to call: put the configuration in adios_config.h, this header
//   fills what was left unsaid. An application Makefile only includes
//   include/makefile/app_config.mk to receive the same answers.
/////////////////////////////////////////////////////////////////////////////

#ifndef _ADIOS_CONFIG_DEFAULTS_H
#define _ADIOS_CONFIG_DEFAULTS_H

#include "adios_config.h"

/////////////////////////////////////////////////////////////////////////////
// ADIOS_USERDATA_PAGES - flash pages at the TOP of memory that belong to
// the application but not to its image (see the linker templates).
//
// A persistent device ID (ADIOS_DEVICE_ID_PERSIST) lives in the last two
// bytes of flash, so SOMETHING has to keep the linker away from the page
// they sit in. An application that says nothing gets the minimum
// automatically; one that reserves pages for its own data keeps its number
// (the record fits inside); one that explicitly says 0 while asking for
// persistence is contradicting itself, and hears about it at compile time.
/////////////////////////////////////////////////////////////////////////////

#ifndef ADIOS_USERDATA_PAGES
# if defined(ADIOS_DEVICE_ID_PERSIST) && ADIOS_DEVICE_ID_PERSIST
#  define ADIOS_USERDATA_PAGES 1
# else
#  define ADIOS_USERDATA_PAGES 0
# endif
#elif defined(ADIOS_DEVICE_ID_PERSIST) && ADIOS_DEVICE_ID_PERSIST && (ADIOS_USERDATA_PAGES < 1)
# error "ADIOS_DEVICE_ID_PERSIST = 1 needs somewhere to write, but ADIOS_USERDATA_PAGES is explicitly 0. Reserve at least one page, or drop the persistence."
#endif

#endif /* _ADIOS_CONFIG_DEFAULTS_H */
