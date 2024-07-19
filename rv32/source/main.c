#include <stdint.h>
#include <stdbool.h>

#include "config.h"

#include "hardware.h"
#include "debug.h"
#include "delay.h"
#include "sdcard.h"
#include "ide.h"
#include "cdda.h"
#include "fatfs.h"
#include "imgfile.h"
#include "timer.h"

#if SD_CD_PIN_ACTIVE_HIGH
#define SDCARD_INSERTED (bit_is_set(SD_CD_PIN, SD_CD_BIT))
#else
#define SDCARD_INSERTED (bit_is_clear(SD_CD_PIN, SD_CD_BIT))
#endif

static bool find_imgfile()
{
  if (fatfs_read_rootdir())
    return true;
  fatfs_reset_filename();
  return fatfs_read_rootdir();
}


void read_toc()
{
  uint8_t s = packet.get_toc.select;
  if (s >= imgheader.num_tocs) {
    service_finish_packet(0x50);
    return;
  }
  memcpy(IDE_DATA_BUFFER, &toc[s], 408);
}

void handle_sdcard()
{

  DEBUG_PUTS("[Card inserted]\n");

  if (sd_init() &&
      fatfs_mount() &&
      find_imgfile() &&
      imgfile_init()) {
    set_disk_type(imgheader.disk_type);
    PORTA = fatfs_filenumber;
    fatfs_next_filename();
  } else {
    fatfs_reset_filename();
    PORTA = ~0;
  }
	
  //load file and get to at offset ACC
  DEBUG_PUTS("Read TOC for more details : ");
  read_toc();
  //dump IDE_DATA_BUFFER to debug
  for(int x = 0; x <256; x++ )
  {
    DEBUG_PUTC(IDE_DATA_BUFFER[x])
  }
  
  DEBUG_PUTS("Read TOC for more details : ");

  while (SDCARD_INSERTED) {
    service_ide();
    service_cdda();
  }

  DEBUG_PUTS("[Card extracted]\n");
  cdda_stop();
  set_disk_type(0xff);
}

int main()
{
  DDRA = 0xff;
  DDRB = 0x00;
  PORTB = 0x00;
  DDRB |= _BV(EMPH_BIT);
  DEBUG_INIT();
  timer_init();
  sei();

  uint8_t leds = 1;

  for (;;) {
    PORTA = leds;
    delayms(250);
    delayms(250);
    if (!(leds <<= 1))
	leds++;

    service_ide();

    if (SDCARD_INSERTED)
      handle_sdcard();
  }

  return 0;
}
