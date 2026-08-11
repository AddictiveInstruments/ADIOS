/* ADIOS - common linker-script body, shared by every supported chip.
 *
 * Not used directly: included by the per-family templates
 * (STM32G0xx.ld.S / STM32F4xx.ld.S), which define the three parameters
 * below for the chip being built, then this file supplies everything else.
 * The families genuinely share this body - it was verified identical
 * across all 60 per-chip scripts this replaced (2026-08-10).
 *
 * Parameters the includer must define:
 *   ADIOS_RAM_LEN         contiguous RAM size, K-suffixed (e.g. 36K)
 *   ADIOS_FLASH_TOTAL_LEN total flash of the chip, K-suffixed
 *   ADIOS_FLASH_BSL_LEN   bootloader region
 *   ADIOS_FLASH_APP_LEN   application region (total minus the bootloader)
 *
 * There is a bootloader either way - what differs is who fixes the boundary
 * between the two FLASH regions. On a DYNAMIC-boundary build the two lines
 * below are placeholders: etc/gen_bsl_boundary.sh rewrites them with the
 * boundary measured from the bootloader's real compiled size. On a STATIC
 * build they are already the final layout, the boundary coming from the
 * project's ADIOS_LD_BSL_BOUNDARY_K.
 */

/* MIOS32_USERDATA_PAGES: flash pages at the TOP of memory that belong to the
   application but not to its image - sound banks, settings, the persistent
   device-ID record. Reserving them HERE is the whole point: the FLASH region
   below is shortened by exactly that much, so an application growing into its
   own data fails to LINK instead of overwriting it at the first write.
   The includer supplies the erase granularity; a family whose sectors are
   unequal (F4: 16K then 64K then 128K, the last one useless for a few bytes)
   declares none and must place such a region elsewhere entirely. */
#ifdef ADIOS_LD_USERDATA_PAGES
# ifndef ADIOS_USERDATA_PAGE_SIZE
#  error "MIOS32_USERDATA_PAGES was requested, but this family's linker template declares no uniform erase-page size. The region cannot simply be carved off the top of flash here."
# endif
# define ADIOS_USERDATA_LEN (ADIOS_LD_USERDATA_PAGES * ADIOS_USERDATA_PAGE_SIZE)
#else
# define ADIOS_USERDATA_LEN 0
#endif

/* Memory Spaces Definitions */
MEMORY
{
  RAM (xrw) :     ORIGIN = 0x20000000, LENGTH = ADIOS_RAM_LEN
#ifdef ADIOS_LD_NO_BOOTLOADER
  /* MIOS32_USE_BOOTLOADER = 0: no reserved bootloader region at all, the
     application owns the whole flash from its base. FLASH_BSL is not even
     declared - the .mios32_bsl output section below is retargeted to FLASH,
     the same way etc/gen_bsl_boundary.sh does it for the bootloader's own
     build (an output section mapped to an undeclared region makes ld warn
     and mis-size its neighbours). */
  FLASH (rx) :    ORIGIN = 0x08000000, LENGTH = ADIOS_FLASH_TOTAL_LEN - ADIOS_USERDATA_LEN
#else
  FLASH_BSL (rx) :    ORIGIN = 0x08000000, LENGTH = ADIOS_FLASH_BSL_LEN
  FLASH (rx) :    ORIGIN = 0x08002800, LENGTH = ADIOS_FLASH_APP_LEN - ADIOS_USERDATA_LEN
#endif
  FLASHB1 (rx) :  ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB0 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB1 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB2 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB3 (rx) : ORIGIN = 0x00000000, LENGTH = 0
}

/* exposes the RAM region size to C code (MIOS32_SYS_RAMSizeGet()) without a
   per-chip #if - see mios32_sys.c */
PROVIDE ( _ram_size = LENGTH(RAM) ) ;

/* total flash of this chip, independent of how it is currently split.
   etc/gen_bsl_boundary.sh reads THIS line to size the application region it
   generates - deliberately explicit, rather than making the script add up
   two region lengths whose written form could drift. */
PROVIDE ( _flash_size = ADIOS_FLASH_TOTAL_LEN ) ;

/* top of the user-mode stack - DERIVED from the RAM size rather than typed
   in per chip. The hand-maintained form silently disagreed with the region
   more than once (STM32F405RG/F407VG declared 192K of RAM - counting the
   64K CCM, which is NOT contiguous, it lives at 0x10000000 - while stacking
   at the 128K mark; ST's own db/mcu says 128K). Deriving it makes that
   class of mismatch impossible. */
_estack = 0x20000000 + ADIOS_RAM_LEN;

/* Reserved stack region. __Stack_Init is the BOTTOM of it, and it is a live
   symbol: the bare-metal canary (MIOS32_CORE_USE_CANARI, see
   programming_models/traditional/main.c) fills from there up to the current
   stack pointer, then checks that first word for corruption.
   Uniform across families since 2026-08-10 - STM32F4xx used to say 128,
   which would have placed __Stack_Init above the stack pointer at startup:
   the fill loop would do nothing and the check would report an immediate,
   bogus stack overflow. Never seen only because bare-metal has not yet run
   on real F4 hardware. */
__Stack_Size = 0x1000 ;
PROVIDE ( _Stack_Size = __Stack_Size ) ;
__Stack_Init = _estack  - __Stack_Size ;
/*"PROVIDE" allows to easily override these values from an object file or the commmand line.*/
PROVIDE ( _Stack_Init = __Stack_Init ) ;

/*
There will be a link error if there is not this amount of RAM free at the end.
*/
_Minimum_Stack_Size = 0x100 ;


/******************************************************************************/
/*                       Peripheral memory map                                */
/******************************************************************************/
/*this allows to compile the ST lib in "non-debug" mode*/


/* Sections Definitions */

SECTIONS
{
    /* the MIOS32 BSL section */
    .mios32_bsl :
    {
	. = ALIGN(4);
        KEEP(*(.mios32_bsl))
	. = ALIGN(4);
#ifdef ADIOS_LD_NO_BOOTLOADER
    } >FLASH        /* stays empty: no image is embedded in this mode */
#else
    } >FLASH_BSL
#endif
 
    /* for Cortex devices, the beginning of the startup code is stored in the .isr_vector section, which goes to FLASH */
    .isr_vector :
    {
	_mios32_sys_isr_vector = . ;
	. = ALIGN(4);
        KEEP(*(.isr_vector))            /* Startup code */
	. = ALIGN(4);
    } >FLASH

    /* used in MIOS32_SYS to set the start vector */
    PROVIDE ( mios32_sys_isr_vector = _mios32_sys_isr_vector );
 
    /* for some STRx devices, the beginning of the startup code is stored in the .flashtext section, which goes to FLASH */
    .flashtext :
    {
	. = ALIGN(4);
        *(.flashtext)            /* Startup code */
	. = ALIGN(4);
    } >FLASH
 
    
       /* the program code is stored in the .text section, which goes to Flash */
        .text :
        {
                . = ALIGN(4);
                CREATE_OBJECT_SYMBOLS
                *(.text .text.* .gnu.linkonce.t.*)
                *(.plt)
                *(.gnu.warning)
                *(.glue_7t) *(.glue_7) *(.vfp11_veneer)
                *(.ARM.extab* .gnu.linkonce.armextab.*)
                *(.gcc_except_table)
        } >FLASH

        .eh_frame_hdr : ALIGN (4)
        {
                KEEP (*(.eh_frame_hdr))
        } >FLASH
        .eh_frame : ALIGN (4)
        {
                KEEP (*(.eh_frame))
        } >FLASH
        
        /* .ARM.exidx is sorted, so has to go in its own output section.  */
        __exidx_start = .;
        .ARM.exidx :
        {
                *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        } >FLASH
        __exidx_end = .;

        .rodata : ALIGN (4)
        {
                *(.rodata .rodata.* .gnu.linkonce.r.*)
                
                . = ALIGN(4);
                KEEP(*(.init))
                
                . = ALIGN(4);
                __preinit_array_start = .;
                KEEP (*(.preinit_array))
                __preinit_array_end = .;
                
                . = ALIGN(4);
                __init_array_start = .;
                KEEP (*(SORT(.init_array.*)))
                KEEP (*(.init_array))
                __init_array_end = .;
                
                . = ALIGN(4);
                KEEP(*(.fini))
                
                . = ALIGN(4);
                __fini_array_start = .;
                KEEP (*(.fini_array))
                KEEP (*(SORT(.fini_array.*)))
                __fini_array_end = .;
                
                . = ALIGN(0x4);
                KEEP (*crtbegin.o(.ctors))
                KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
                KEEP (*(SORT(.ctors.*)))
                KEEP (*crtend.o(.ctors))
                
                . = ALIGN(0x4);
                KEEP (*crtbegin.o(.dtors))
                KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
                KEEP (*(SORT(.dtors.*)))
                KEEP (*crtend.o(.dtors))
                
                *(.init .init.*)
                *(.fini .fini.*)
                
                PROVIDE_HIDDEN (__preinit_array_start = .);
                KEEP (*(.preinit_array))
                PROVIDE_HIDDEN (__preinit_array_end = .);
                PROVIDE_HIDDEN (__init_array_start = .);
                KEEP (*(SORT(.init_array.*)))
                KEEP (*(.init_array))
                PROVIDE_HIDDEN (__init_array_end = .);
                PROVIDE_HIDDEN (__fini_array_start = .);
                KEEP (*(.fini_array))
                KEEP (*(SORT(.fini_array.*)))
                PROVIDE_HIDDEN (__fini_array_end = .);
                
                . = ALIGN (8);
                *(.rom)
                *(.rom.b)
                _etext = .;
                /* This is used by the startup in order to initialize the .data secion */
                _sidata = _etext;
        } >FLASH

        /* This is the initialized data section
        The program executes knowing that the data is in the RAM
        but the loader puts the initial values in the FLASH (inidata).
        It is one task of the startup to copy the initial values from FLASH to RAM. */
        .data : ALIGN (8)
        {
                __ram_start = .;
                /* This is used by the startup in order to initialize the .data section */
                _sdata = . ;
                
                . = ALIGN(0x80);
                _isr_vectorsram_offs = . - 0x20000000;
                KEEP(*(.isr_vectorsram))
                
                . = ALIGN(4);
                KEEP(*(.jcr))
                *(.got.plt) *(.got)
                *(.shdata)
                *(.data .data.* .gnu.linkonce.d.*)
                . = ALIGN (8);
                *(.ram)
                *(.ramfunc*)
                 . = ALIGN(4);
                /* This is used by the startup in order to initialize the .data section */
                _edata = .;
        } >RAM AT>FLASH

        /* This is the uninitialized data section */
        .bss (NOLOAD):
        {
                . = ALIGN(4);
                /* This is used by the startup in order to initialize the .bss section */
                _sbss = .;
                *(.shbss)
                *(.bss .bss.* .gnu.linkonce.b.*)
                *(COMMON)
                . = ALIGN (8);
                *(.ram.b)
                . = ALIGN(4);
                 /* This is used by the startup in order to initialize the .bss section */
                 _ebss = . ;
                _end = .;
        } >RAM AT>FLASH

    PROVIDE ( end = _ebss );
    PROVIDE ( _end = _ebss );
    PROVIDE ( __bss_start__ = _sbss );
    PROVIDE ( __bss_end__ = _ebss );


    /* This is the user stack section 
    This is just to check that there is enough RAM left for the User mode stack
    It should generate an error if it's full.
     */
    ._usrstack (NOLOAD):
    {
        . = ALIGN(4);
        _susrstack = . ;
        . = . + _Minimum_Stack_Size ;
        . = ALIGN(4);
        _eusrstack = . ;
        __ram_end = .;
    } >RAM
    

   
    /* this is the FLASH Bank1 */
    /* the C or assembly source must explicitly place the code or data there
    using the "section" attribute */
    .b1text :
    {
        *(.b1text)                   /* remaining code */
        *(.b1rodata)                 /* read-only data (constants) */
        *(.b1rodata*)
    } >FLASHB1
    
    /* this is the EXTMEM */
    /* the C or assembly source must explicitly place the code or data there
    using the "section" attribute */
    
    /* EXTMEM Bank0 */
    .eb0text :
    {
        *(.eb0text)                   /* remaining code */
        *(.eb0rodata)                 /* read-only data (constants) */
        *(.eb0rodata*)
    } >EXTMEMB0
    
    /* EXTMEM Bank1 */
    .eb1text :
    {
        *(.eb1text)                   /* remaining code */
        *(.eb1rodata)                 /* read-only data (constants) */
        *(.eb1rodata*)
    } >EXTMEMB1
    
    /* EXTMEM Bank2 */
    .eb2text :
    {
        *(.eb2text)                   /* remaining code */
        *(.eb2rodata)                 /* read-only data (constants) */
        *(.eb2rodata*)
    } >EXTMEMB2
    
    /* EXTMEM Bank0 */
    .eb3text :
    {
        *(.eb3text)                   /* remaining code */
        *(.eb3rodata)                 /* read-only data (constants) */
        *(.eb3rodata*)
    } >EXTMEMB3
    
    /* after that it's only debugging information. */
    
    /* remove the debugging information from the standard libraries */
    /DISCARD/ :
    {
     libc.a ( * )
     libm.a ( * )
     libgcc.a ( * )
     }

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /* DWARF debug sections.
       Symbols in the DWARF debugging sections are relative to the beginning
       of the section so we begin them at 0.  */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
}



