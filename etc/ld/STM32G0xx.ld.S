/* ADIOS - linker script template for the STM32G0xx family.
 *
 * Replaces the STM32G0*.ld files that were maintained one per chip: only
 * two numbers ever differed between them - contiguous RAM and total flash -
 * so those live in the table below, and everything else comes from
 * adios_body.ld.inc, shared with the other family.
 *
 * Preprocessed by the build:
 *   arm-none-eabi-gcc -E -x assembler-with-cpp -P -DADIOS_LD_CHIP_<chip>
 * Both candidate keys are passed - the exact part number and the package-
 * less form - and the table matches whichever it carries, reproducing the
 * old exact-then-fallback filename lookup. No chip has both forms in the
 * table, so they can never collide.
 *
 * RAM and flash figures cross-checked against ST's own MCU database
 * (CubeMX db/mcu/*.xml, <Ram>/<Flash> tags) on 2026-08-10. That pass found
 * STM32F405RG/F407VG claiming 192K of RAM - the 64K CCM counted in, though
 * it is not contiguous - now corrected to the real 128K.
 */

/* Bootloader region. A project building with ADIOS_USE_DYNAMIC_BSL_BOUNDARY
   gets this overwritten with the boundary measured from its bootloader's
   real size; a static build keeps exactly this value, and can override it
   from its Makefile (ADIOS_LD_BSL_BOUNDARY_K). Default = what every
   STM32G0 script carried before this consolidation. */
#ifndef ADIOS_LD_BSL_BOUNDARY_K
# define ADIOS_LD_BSL_BOUNDARY_K 10
#endif

#if defined(ADIOS_LD_CHIP_STM32G030K6)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G030x8)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G031K8)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G031x4)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 16K
#elif defined(ADIOS_LD_CHIP_STM32G031x6)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G041x6)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G041x8)
# define ADIOS_RAM_LEN         8K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G050K8)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G050x6)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G051x6)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G051x8)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G061x6)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G061x8)
# define ADIOS_RAM_LEN         18K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G070CB)
# define ADIOS_RAM_LEN         36K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32G071x6)
# define ADIOS_RAM_LEN         36K
# define ADIOS_FLASH_TOTAL_LEN 32K
#elif defined(ADIOS_LD_CHIP_STM32G071x8)
# define ADIOS_RAM_LEN         36K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32G071xB)
# define ADIOS_RAM_LEN         36K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32G081xB)
# define ADIOS_RAM_LEN         36K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32G0B0xE)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32G0B1xB)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32G0B1xC)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 256K
#elif defined(ADIOS_LD_CHIP_STM32G0B1xE)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32G0C1xC)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 256K
#elif defined(ADIOS_LD_CHIP_STM32G0C1xE)
# define ADIOS_RAM_LEN         144K
# define ADIOS_FLASH_TOTAL_LEN 512K
#else
# error "Unknown chip for the STM32G0xx linker template - add its entry above (RAM and total flash from ST's db/mcu), or check the -DADIOS_LD_CHIP_... passed by the build."
#endif

#define ADIOS_FLASH_BSL_LEN (ADIOS_LD_BSL_BOUNDARY_K * 1K)
#define ADIOS_FLASH_APP_LEN (ADIOS_FLASH_TOTAL_LEN - ADIOS_FLASH_BSL_LEN)

/* erase granularity, needed only to size a ADIOS_USERDATA_PAGES region:
   uniform 2K page-erase across the whole G0 family. A family whose sectors
   are unequal must NOT define this - see adios_body.ld.inc. */
#define ADIOS_USERDATA_PAGE_SIZE 2048

#include "adios_body.ld.inc"
