/* ADIOS - linker script template for the STM32F4xx family.
 *
 * Replaces the STM32F4*.ld files that were maintained one per chip: only
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

/* Bootloader region. A project building with MIOS32_USE_DYNAMIC_BSL_BOUNDARY
   gets this overwritten with the boundary measured from its bootloader's
   real size; a static build keeps exactly this value, and can override it
   from its Makefile (ADIOS_LD_BSL_BOUNDARY_K). Default = what every
   STM32F4 script carried before this consolidation. */
#ifndef ADIOS_LD_BSL_BOUNDARY_K
# define ADIOS_LD_BSL_BOUNDARY_K 16
#endif

#if defined(ADIOS_LD_CHIP_STM32F401xB)
# define ADIOS_RAM_LEN         64K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32F401xC)
# define ADIOS_RAM_LEN         64K
# define ADIOS_FLASH_TOTAL_LEN 256K
#elif defined(ADIOS_LD_CHIP_STM32F401xD)
# define ADIOS_RAM_LEN         96K
# define ADIOS_FLASH_TOTAL_LEN 384K
#elif defined(ADIOS_LD_CHIP_STM32F401xE)
# define ADIOS_RAM_LEN         96K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F405RG)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F405xE)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F407VG)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F407xE)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F410x8)
# define ADIOS_RAM_LEN         32K
# define ADIOS_FLASH_TOTAL_LEN 64K
#elif defined(ADIOS_LD_CHIP_STM32F410xB)
# define ADIOS_RAM_LEN         32K
# define ADIOS_FLASH_TOTAL_LEN 128K
#elif defined(ADIOS_LD_CHIP_STM32F411xC)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 256K
#elif defined(ADIOS_LD_CHIP_STM32F411xE)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F412xE)
# define ADIOS_RAM_LEN         256K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F412xG)
# define ADIOS_RAM_LEN         256K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F413xG)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F413xH)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 1536K
#elif defined(ADIOS_LD_CHIP_STM32F415xG)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F417xE)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F417xG)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F423xH)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 1536K
#elif defined(ADIOS_LD_CHIP_STM32F427xG)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F427xI)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#elif defined(ADIOS_LD_CHIP_STM32F429xE)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F429xG)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F429xI)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#elif defined(ADIOS_LD_CHIP_STM32F437xG)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F437xI)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#elif defined(ADIOS_LD_CHIP_STM32F439xG)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F439xI)
# define ADIOS_RAM_LEN         192K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#elif defined(ADIOS_LD_CHIP_STM32F446xC)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 256K
#elif defined(ADIOS_LD_CHIP_STM32F446xE)
# define ADIOS_RAM_LEN         128K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F469xE)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 512K
#elif defined(ADIOS_LD_CHIP_STM32F469xG)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F469xI)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#elif defined(ADIOS_LD_CHIP_STM32F479xG)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 1024K
#elif defined(ADIOS_LD_CHIP_STM32F479xI)
# define ADIOS_RAM_LEN         320K
# define ADIOS_FLASH_TOTAL_LEN 2048K
#else
# error "Unknown chip for the STM32F4xx linker template - add its entry above (RAM and total flash from ST's db/mcu), or check the -DADIOS_LD_CHIP_... passed by the build."
#endif

#define ADIOS_FLASH_BSL_LEN (ADIOS_LD_BSL_BOUNDARY_K * 1K)
#define ADIOS_FLASH_APP_LEN (ADIOS_FLASH_TOTAL_LEN - ADIOS_FLASH_BSL_LEN)

#include "adios_body.ld.inc"
