#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "config.h"

#include "imgfile.h"
#include "fatfs.h"
#include "hardware.h"
#include "debug.h"
#include "cdda.h"

struct imgheader imgheader;
struct toc toc[2];

static struct fatfs_handle read_handle;
static struct fatfs_handle cdda_handle;

uint8_t imgfile_data_offs;
uint8_t imgfile_data_len;
static uint8_t imgfile_skip_before, imgfile_skip_after;
static uint16_t imgfile_sector_size, imgfile_sector_completed;
bool imgfile_need_to_read;

void printLinePrefix (uint32_t number)
{
  char paddedString[] = "0000000";
  char numString [8];

  DEBUG_PUTS("\n[");

  if(number < 10)
  {
    DEBUG_PUTS("000000");
  }

  if(number < 100 && number >= 10)
  {
    DEBUG_PUTS("00000");
  }

  if(number < 1000 && number >= 100)
  {
    DEBUG_PUTS("0000");
  }

  if(number < 10000 && number >= 1000)
  {
    DEBUG_PUTS("000");
  }

  if(number < 100000 && number >= 10000)
  {
    DEBUG_PUTS("00");
  }
  
  if(number < 1000000 && number >= 100000)
  {
    DEBUG_PUTS("0");
  }


  DEBUG_PUTX((uint8_t)(number>>24));
  DEBUG_PUTX((uint8_t)(number>>16));
  DEBUG_PUTX((uint8_t)(number>>8));
  DEBUG_PUTX((uint8_t)number);

  DEBUG_PUTS("] ");
}

bool imgfile_init()
{
  read_handle.pos = cdda_handle.pos = ~0;

  if (!fatfs_read_header(&imgheader, sizeof(imgheader), 0) ||
      imgheader.magic != HEADER_MAGIC ||
      imgheader.num_tocs < 1 || imgheader.num_tocs > 2)
    return false;

  uint8_t i;
  struct toc *t = &toc[0];
  for (i=0; i<imgheader.num_tocs; i++) {
    if (!fatfs_read_header(t++, sizeof(*t), i+1))
      return false;
  }

  DEBUG_PUTS("ImgFile Initialised\n");
  
  uint32_t x;
  char linePrefix [10];
  //sprintf(0,"00000000%d",linePrefix);
  const unsigned char * const test = (unsigned char*)&toc[0];
  DEBUG_PUTS("Size of TOC array..");
  unsigned int sizeOfToc = sizeof(toc[0]);
  printLinePrefix(sizeOfToc);
  //DEBUG_PUTX(sizeOfToc);
  printLinePrefix(0);
  
  for(x = 0xACA10; x < 0xACAA0; x++)
  {
    DEBUG_PUTX(test[x]);
    DEBUG_PUTS(" ");
    if((x+1) % 16 == 0)
    {
      printLinePrefix(x);
    }
  }
  DEBUG_PUTS("\n");
  
  return true;
}

static void imgfile_adjust_sector_start()
{
  if (imgfile_skip_before > (uint8_t)~imgfile_data_offs) {
    if (imgfile_need_to_read)
      fatfs_read_next_sector(&read_handle, 0);
    imgfile_need_to_read = true;
  }
  imgfile_data_offs += imgfile_skip_before;
}

bool imgfile_read_next_sector(uint8_t *ptr)
{
  if (imgfile_need_to_read) {
    if (!fatfs_read_next_sector(&read_handle, ptr))
      return false;
    imgfile_need_to_read = false;
  }
  if (((uint8_t)~imgfile_data_offs) < (imgfile_sector_size - imgfile_sector_completed)) {
    imgfile_data_len = ((uint8_t)~imgfile_data_offs)+1;
  } else {
    imgfile_data_len = imgfile_sector_size - imgfile_sector_completed;
  }
  return true;
}

bool imgfile_sector_complete()
{
  imgfile_sector_completed += (imgfile_data_len? imgfile_data_len : 512/2);
  imgfile_data_offs += imgfile_data_len;
  if (!imgfile_data_offs)
    imgfile_need_to_read = true;
  if (imgfile_sector_completed >= imgfile_sector_size) {
    imgfile_sector_completed = 0;
    if (imgfile_skip_after > (uint8_t)~imgfile_data_offs) {
      if (imgfile_need_to_read)
	fatfs_read_next_sector(&read_handle, 0);
      imgfile_need_to_read = true;
    }
    imgfile_data_offs += imgfile_skip_after;
    imgfile_adjust_sector_start();
    return true;
  }
  return false;
}

bool imgfile_read_next_sector_cdda(uint8_t idx)
{
  return fatfs_read_next_sector(&cdda_handle, &CDDA_DATA_BUFFER[(idx? 512:0)]);
}

static bool imgfile_seek_internal(uint32_t sec, uint8_t mode, bool data)
{
  uint8_t i;
  uint32_t blk;
  uint8_t rmode = 0xff;
  uint8_t secoffs = 0;
  for(i=0; i<imgheader.num_regions; i++) {
    uint32_t start = imgheader.regions[i].start_and_type & 0xffffff;
    if (sec >= start) {
      rmode = imgheader.regions[i].start_and_type >> 24;
      uint32_t wordoffs = (sec-start)*(2352/2);
      secoffs = (uint8_t)wordoffs;
      blk = (wordoffs>>8)+imgheader.regions[i].fileoffs;
    } else {
      if (!i)
	return false;
      break;
    }
  }
  uint8_t skip_before = 0, skip_after = 0;
  switch((mode>>1)&7) {
  case 0:
    if(!(mode & 0x10)) {
      /* Data select with "Any type"; check for data track and assume
	 mode2/form1 for XA and mode1 otherwise */
      if (!(rmode & 4))
	return false;
      if (imgheader.disk_type == 0x20) {
	skip_after = 280/2;
	if (!(mode & 0x40))
	  skip_before = 8/2;
      } else
	skip_after = 288/2;
    }
    break;
  case 1:
    if (rmode & 4)
      return false;
    break;
  case 2:
    skip_after = 288/2;
  case 3:
    /* FALLTHRU */
    if (!(rmode & 4) || imgheader.disk_type == 0x20)
      return false;
    break;
  case 4:
    skip_after = 276/2;
    /* FALLTHRU */
  case 5:
    skip_after += 4/2;
    if (!(rmode & 4) || imgheader.disk_type != 0x20)
      return false;
    if (!(mode & 0x40))
      skip_before = 8/2;
    break;
  case 6:
    break;
  default:
    return false;
  }
  if (mode & 0x10) {
    skip_before = 0;
    skip_after = 0;
  } else if (!(mode & 0x20)) {
    return false;
  } else if (!(mode & 0x80)) {
    skip_before += 16/2;
  }
  if (data) {
    imgfile_sector_size = 2352/2;
    imgfile_sector_size -= skip_before;
    imgfile_sector_size -= skip_after;
    imgfile_data_offs = secoffs;
    imgfile_skip_before = skip_before;
    imgfile_skip_after = skip_after;
    imgfile_need_to_read = false;
    imgfile_sector_completed = 0;
    imgfile_adjust_sector_start();
    if (imgfile_need_to_read)
      blk++;
    else
      imgfile_need_to_read = true;
  } else {
    cdda_subcode_q[0] = (rmode<<4)|1;
    cdda_toc = rmode & 0x80;
  }
  return fatfs_seek((data? &read_handle : &cdda_handle), blk);
}

bool imgfile_seek(uint32_t sec, uint8_t mode)
{
  if (!imgfile_seek_internal(sec, mode, true)) {
    DEBUG_PUTS("SEEK mode=");
    DEBUG_PUTX(mode);
    DEBUG_PUTS(" failed\n");
    return false;
  }
  return true;
}

bool imgfile_seek_cdda(uint32_t sec)
{
  return imgfile_seek_internal(sec, 0x12, false);
}

