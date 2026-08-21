//! \defgroup ADIOS_SPI_MIDI
//!
//! SPI MIDI layer for ADIOS
//! 
//! Applications shouldn't call these functions directly, instead please
//! use \ref ADIOS_MIDI layer functions
//! 
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>

// To use it, declare ADIOS_USE_SPI_MIDI in your project's
// adios_config.h. The MIDI dispatcher guards its SPIM branches with the
// same symbol, so the ports appear and disappear together with this file.
#if defined(ADIOS_USE_SPI_MIDI)

// The link needs its bus. Same guard as sdcard and srio: one sentence at
// compile time instead of four "undefined reference to ADIOS_SPI_..." later.
#if ADIOS_SPI_MIDI_NUM_PORTS > 0
# if ADIOS_SPI_MIDI_SPI == 0 && !defined(ADIOS_USE_SPI0)
#  error "ADIOS_USE_SPI_MIDI needs its SPI port: add #define ADIOS_USE_SPI0 to your adios_config.h, or point ADIOS_SPI_MIDI_SPI at another port."
# elif ADIOS_SPI_MIDI_SPI == 1 && !defined(ADIOS_USE_SPI1)
#  error "ADIOS_USE_SPI_MIDI needs its SPI port: add #define ADIOS_USE_SPI1 to your adios_config.h, or point ADIOS_SPI_MIDI_SPI at another port."
# elif ADIOS_SPI_MIDI_SPI == 2 && !defined(ADIOS_USE_SPI2)
#  error "ADIOS_USE_SPI_MIDI needs its SPI port: add #define ADIOS_USE_SPI2 to your adios_config.h, or point ADIOS_SPI_MIDI_SPI at another port."
# elif ADIOS_SPI_MIDI_SPI > 2
#  error "ADIOS_SPI_MIDI_SPI points at a port that does not exist."
# endif
#endif


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#if !defined(ADIOS_SPI_MIDI_MUTEX_TAKE)
#define ADIOS_SPI_MIDI_USE_MUTEX 0
#define ADIOS_SPI_MIDI_MUTEX_TAKE {}
#define ADIOS_SPI_MIDI_MUTEX_GIVE {}
#else
#define ADIOS_SPI_MIDI_USE_MUTEX 1
#endif


/////////////////////////////////////////////////////////////////////////////
// Local Variables
/////////////////////////////////////////////////////////////////////////////

#if ADIOS_SPI_MIDI_NUM_PORTS > 0
// TX double buffer toggles between each scan
static u32 tx_upstream_buffer[2][ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE];
static u8 tx_upstream_buffer_select;

// RX downstream buffer used to temporary store new words from current scan
static u32 rx_downstream_buffer[ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE];

// RX ring buffer
static u32 rx_ringbuffer[ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE];

#if ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE > 255 || ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE > 255
# error "Please adapt size pointers!"
#endif
static u8 tx_buffer_head;

static u8 rx_ringbuffer_tail;
static u8 rx_ringbuffer_head;
static u8 rx_ringbuffer_size;

// indicates ongoing scan
static volatile u8 transfer_done;

// optional hook: a board driver can claim raw words before MIDI parsing
static s32 (*raw_word_callback_func)(u32 word);
#endif


/////////////////////////////////////////////////////////////////////////////
// Local Prototypes
/////////////////////////////////////////////////////////////////////////////

#if ADIOS_SPI_MIDI_NUM_PORTS > 0
static s32 ADIOS_SPI_MIDI_InitScanBuffer(u32 *buffer);
static void ADIOS_SPI_MIDI_DMA_Callback(void);
#endif


/////////////////////////////////////////////////////////////////////////////
// Initializes the SPI Master
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_Init(u32 mode)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return -1; // SPI MIDI not activated
#else
  s32 status = 0;

  if( mode != 0 )
    return -1; // currently only mode 0 supported

  if( !ADIOS_SPI_MIDI_Enabled() )
    return -3; // SPI MIDI device hasn't been enabled in ADIOS bootloader

  // deactivate CS output
  ADIOS_SPI_CS_PinSet(ADIOS_SPI_MIDI_SPI, 1);

  // ensure that fast pin drivers are activated
  ADIOS_SPI_IO_Init(ADIOS_SPI_MIDI_SPI, ADIOS_SPI_PIN_DRIVER_STRONG);

  // starting with first half of the double buffer
  tx_upstream_buffer_select = 0;

  // last transfer done (to allow next transfer)
  transfer_done = 1;

  // init buffer pointers
  tx_buffer_head = 0;
  rx_ringbuffer_tail = rx_ringbuffer_head = rx_ringbuffer_size = 0;

  // init double buffers
  ADIOS_SPI_MIDI_InitScanBuffer((u32 *)&tx_upstream_buffer[0]);
  ADIOS_SPI_MIDI_InitScanBuffer((u32 *)&tx_upstream_buffer[1]);

  // no board driver has claimed the raw words yet - one registers itself
  // with ADIOS_SPI_MIDI_RawWordCallback_Init(), typically from its own
  // Init(), which the application calls after this one. (The M16's start-up
  // sequence used to sit right here behind an #ifdef; it is now
  // ADIOS_SPIM_M16_Init() in modules/m16.)
  raw_word_callback_func = NULL;

  return status;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! \returns != 0 if SPI MIDI has been enabled in ADIOS bootloader
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_Enabled(void)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return 0; // SPI MIDI interface not explicitely enabled in adios_config.h
#else
  u8 *spi_midi_confirm = (u8 *)ADIOS_SYS_ADDR_SPI_MIDI_CONFIRM;
  u8 *spi_midi = (u8 *)ADIOS_SYS_ADDR_SPI_MIDI;
  if( *spi_midi_confirm == 0x42 && *spi_midi < 0x80 )
    return *spi_midi;

  return 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! This function checks the availability of a SPI MIDI port as configured
//! with ADIOS_SPI_MIDI_NUM_PORTS
//!
//! \param[in] spi_midi_port module number (0..7)
//! \return 1: interface available
//! \return 0: interface not available
//! \note Applications shouldn't call this function directly, instead please use \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_CheckAvailable(u8 spi_midi_port)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return 0; // SPI MIDI interface not explicitely enabled in adios_config.h
#else
  return (spi_midi_port < ADIOS_SPI_MIDI_NUM_PORTS) ? ADIOS_SPI_MIDI_Enabled() : 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! Installs an optional callback which is given every received word BEFORE
//! it is read as a MIDI package.
//!
//! This is how a board at the far end of the link gets its own protocol
//! through without the transport knowing anything about it. The M16 uses it
//! for its status channel; until 2026-08-14 that test sat hard-wired inside
//! ADIOS_SPI_MIDI_Periodic_mS() behind an #ifdef.
//!
//! \param[in] callback_raw_word the callback, or NULL to remove it. It must
//!            return 1 if it consumed the word, 0 to let it through.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_RawWordCallback_Init(s32 (*callback_raw_word)(u32 word))
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return -1; // SPI MIDI not activated
#else
  raw_word_callback_func = callback_raw_word;

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! This function should be called periodically each mS to initiate a new
//! SPI scan
//!
//! Not for use in an application - this function is called from
//! ADIOS_MIDI_Periodic_mS(), which is called by a task in the programming
//! model!
//! 
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_Periodic_mS(void)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return 0; // SPI MIDI not activated (no error)
#else
  if( !ADIOS_SPI_MIDI_Enabled() ){
	  //DEBUG_MSG("poll,err:-3");
    return -3; // SPI MIDI device hasn't been enabled in ADIOS bootloader
  }
  if( !transfer_done ){
	  //DEBUG_MSG("poll,err:-2");
    return -2; // previous transfer not finished yet
  }
  // following operation should be atomic
  ADIOS_IRQ_Disable();

  // last TX buffer
  u32 *last_tx = (u32 *)&tx_upstream_buffer[tx_upstream_buffer_select];

  // next TX buffer
  tx_upstream_buffer_select = tx_upstream_buffer_select ? 0 : 1;
  u32 *next_tx = (u32 *)&tx_upstream_buffer[tx_upstream_buffer_select];

  // init last TX buffer for next words
  ADIOS_SPI_MIDI_InitScanBuffer(last_tx);
  tx_buffer_head = 0;

  // start next transfer
  transfer_done = 0;

  ADIOS_IRQ_Enable();

  // take over access over SPI port
  ADIOS_SPI_MIDI_MUTEX_TAKE;

  // init SPI
  ADIOS_SPI_TransferModeInit(ADIOS_SPI_MIDI_SPI,
			      ADIOS_SPI_MODE_CLK1_PHASE1,
			      ADIOS_SPI_MIDI_SPI_PRESCALER);

  // activate CS output
  ADIOS_SPI_CS_PinSet(ADIOS_SPI_MIDI_SPI, 0);

  // start next transfer
  ADIOS_SPI_TransferBlock(ADIOS_SPI_MIDI_SPI,
			   (u8 *)next_tx, (u8 *)rx_downstream_buffer,
			   4*ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE,
			   ADIOS_SPI_MIDI_DMA_Callback);

#if ADIOS_SPI_MIDI_USE_MUTEX
  // workaround - search for a better way to release mutex from ISR
  // it's currently not possible to release it from ADIOS_SPI_MIDI_DMA_Callback()
  while( !transfer_done );
  ADIOS_SPI_MIDI_MUTEX_GIVE;
#endif

  return 0; // no error
#endif
}


/////////////////////////////////////////////////////////////////////////////
// Called after DMA transfer finished
/////////////////////////////////////////////////////////////////////////////
#if ADIOS_SPI_MIDI_NUM_PORTS > 0
static void ADIOS_SPI_MIDI_DMA_Callback(void)
{
  // deactivate CS output
  ADIOS_SPI_CS_PinSet(ADIOS_SPI_MIDI_SPI, 1);

  // release access over SPI port
  //ADIOS_SPI_MIDI_MUTEX_GIVE;
  // doesn't work - see ADIOS_SPI_MIDI_USE_MUTEX workaround in ADIOS_SPI_MIDI_Periodic_mS

  // transfer RX values into ringbuffer (if possible)
  if( rx_ringbuffer_size < ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE ) {
    int i;

    // atomic operation to avoid conflict with other interrupts
    ADIOS_IRQ_Disable();

    // search for valid MIDI events in downstream buffer, and put them into the receive ringbuffer
    u32 *rx_buffer = (u32 *)&rx_downstream_buffer[0];
    for(i=0; i<ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE; ++i) {
      u32 word = *rx_buffer++;

      if( word != 0xffffffff && word != 0x00000000 ) {

		// give a board driver the chance to claim this word before we
		// read it as MIDI - see ADIOS_SPI_MIDI_RawWordCallback_Init().
		// A board that carries its own protocol alongside MIDI claims the
		// words it recognises here, typically by their CIN.
		if( raw_word_callback_func == NULL || raw_word_callback_func(word) == 0 ){
	    	adios_midi_package_t p;
			p.cin_cable = word >> 24;
			p.evnt0 = word >> 16;
			p.evnt1 = word >> 8;
			p.evnt2 = word >> 0;
			rx_ringbuffer[rx_ringbuffer_head] = p.ALL;

			if( ++rx_ringbuffer_head >= ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE )
			  rx_ringbuffer_head = 0;

			if( ++rx_ringbuffer_size >= ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE )
			  break; // ringbuffer full :-( - TODO: add rx error counter
		      }
		    }
		}

    ADIOS_IRQ_Enable();
  }

  // transfer finished
  transfer_done = 1;
}
#endif


/////////////////////////////////////////////////////////////////////////////
// This function puts a new MIDI package into the Tx buffer
// \param[in] package MIDI package
// \return 0: no error
// \return -1: SPI not configured
// \return -2: buffer is full
//             caller should retry until buffer is free again
// \note Applications shouldn't call this function directly, instead please use \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_PackageSend_NonBlocking(adios_midi_package_t package)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return -1; // SPI MIDI not activated
#else
  if( !ADIOS_SPI_MIDI_Enabled() ){
    return -3; // SPI MIDI device hasn't been enabled in ADIOS bootloader
  }
  // buffer full?
  if( tx_buffer_head >= ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE ) {
    // flush buffer if possible
    // (this call simplifies polling loops!)
    ADIOS_SPI_MIDI_Periodic_mS();

    // notify that buffer was full (request retry)
    return -2;
  }

  // since data will be transmitted bytewise, we've to swap the order
  u32 word = (package.cin_cable << 24) | (package.evnt0 << 16) | (package.evnt1 << 8) | (package.evnt2 << 0);

  // put package into buffer - this operation should be atomic!
  ADIOS_IRQ_Disable();
  u8 next_select = tx_upstream_buffer_select ? 0 : 1;
  tx_upstream_buffer[next_select][tx_buffer_head] = word;
  ++tx_buffer_head;
  ADIOS_IRQ_Enable();

  return 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
//! This function puts a new MIDI package into the Tx buffer
//! (blocking function)
//! \param[in] package MIDI package
//! \return 0: no error
//! \return -1: SPI not configured
//! \note Applications shouldn't call this function directly, instead please use \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_PackageSend(adios_midi_package_t package)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return -1; // SPI MIDI not activated
#else
  if( !ADIOS_SPI_MIDI_Enabled() )
    return -3; // SPI MIDI device hasn't been enabled in ADIOS bootloader

  static u16 timeout_ctr = 0;
  // this function could hang up if SPI receive buffer not empty and data
  // should be sent.
  // Therefore we time out the polling after 10000 tries
  // Once the timeout value is reached, each new MIDI_PackageSend call will
  // try to access the SPI only a single time anymore. Once the try
  // was successfull (transfer done and receive buffer empty), timeout value is
  // reset again

  s32 error;

  while( (error=ADIOS_SPI_MIDI_PackageSend_NonBlocking(package)) == -2 ) {
    if( timeout_ctr >= 10000 )
      break;
    ++timeout_ctr;
  }

  if( error >= 0 ) // no error: reset timeout counter
    timeout_ctr = 0;

  return error;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! This function checks for a new package
//! \param[out] package pointer to MIDI package (received package will be put into the given variable)
//! \return -1 if no package in buffer
//! \return >= 0: number of packages which are still in the buffer
//! \note Applications shouldn't call this function directly, instead please use \ref ADIOS_MIDI layer functions
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SPI_MIDI_PackageReceive(adios_midi_package_t *package)
{
#if ADIOS_SPI_MIDI_NUM_PORTS == 0
  return -1; // SPI MIDI not activated
#else
  if( !ADIOS_SPI_MIDI_Enabled() )
    return -3; // SPI MIDI device hasn't been enabled in ADIOS bootloader

  // package received?
  if( !rx_ringbuffer_size )
    return -1;

  // get package - this operation should be atomic!
  ADIOS_IRQ_Disable();
  package->ALL = rx_ringbuffer[rx_ringbuffer_tail];
  if( ++rx_ringbuffer_tail >= ADIOS_SPI_MIDI_RX_RINGBUFFER_SIZE )
    rx_ringbuffer_tail = 0;
  --rx_ringbuffer_size;
  ADIOS_IRQ_Enable();

  return rx_ringbuffer_size;
#endif
}



/////////////////////////////////////////////////////////////////////////////
// Invalidates a buffer with all-1
/////////////////////////////////////////////////////////////////////////////
#if ADIOS_SPI_MIDI_NUM_PORTS > 0
static s32 ADIOS_SPI_MIDI_InitScanBuffer(u32 *buffer)
{
  int i;

  for(i=0; i<ADIOS_SPI_MIDI_SCAN_BUFFER_SIZE; ++i) {
    *buffer++ = 0xffffffff;
  }

  return 0; // no error
}
#endif

//! \}

#endif /* ADIOS_USE_SPI_MIDI */
