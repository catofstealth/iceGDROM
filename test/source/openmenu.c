#include <stdio.h>
#include <stdint.h>
#include "cdops.h"

extern unsigned long Timer( );

static struct TOC toc;
static unsigned int track_start;

static char bigbuf[2048*1024];

static int select_track()
{
  unsigned char disc_status;
  if (cdops_init_drive() < 0) {
    printf("Failed to init drive%s!\n", ((cdops_disc_status()&0xf)==6? " (no disc in drive)":""));
    return 0;
  }
  disc_status = cdops_disc_status();
  if (cdops_read_toc(&toc, ((disc_status&0x80)? 1 : 0)) < 0) {
    printf("Failed to read TOC!\n");
    return 0;
  }
  unsigned i, first = TOC_TRACK(toc.first), last = TOC_TRACK(toc.last);
  if (first < 1) first = 1;
  for (i=last; i>=first; --i) {
    if (TOC_CTRL(toc.entry[i-1])&4) {
      track_start = TOC_LBA(toc.entry[i-1]);
      printf("Using track %d (in %s density region), LBA %d\n",
	     (int)i, ((disc_status&0x80)? "high" : "low"), (int)track_start);
      return 1;
    }
  }
  printf("No suitable track found\n");
  return 0;
}

static int select_track_manual_toc()
{
  unsigned char disc_status;
  if (cdops_init_drive() < 0) {
    printf("Failed to init drive%s!\n", ((cdops_disc_status()&0xf)==6? " (no disc in drive)":""));
    return 0;
  }
  disc_status = cdops_disc_status();

  struct { int session; void *buffer; } param;
  param.session = ((disc_status&0x80)? 1 : 0);
  param.buffer = &toc;
  if(cdops_exec_cmd(19, &param) < 0)
  {
	printf("Failed to read TOC!\n");
  	return 0;	  
  }
  //if (cdops_read_toc(&toc, ((disc_status&0x80)? 1 : 0)) < 0) {
  //  printf("Failed to read TOC!\n");
  //  return 0;
  //}


	
  unsigned i, first = TOC_TRACK(toc.first), last = TOC_TRACK(toc.last);
  if (first < 1) first = 1;
  for (i=last; i>=first; --i) {
    if (TOC_CTRL(toc.entry[i-1])&4) {
      track_start = TOC_LBA(toc.entry[i-1]);
      printf("Using track %d (in %s density region), LBA %d\n",
	     (int)i, ((disc_status&0x80)? "high" : "low"), (int)track_start);
      return 1;
    }
  }
  printf("No suitable track found\n");
  return 0;
}

int test_get_ver()
{
  printf("Testing cmd 0x52, 0x00 (Get ODE Version)\n");
  //uint8_t params[11] = { 0x00, 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  cdops_exec_cmd(0x52, 0); //fire and forget, though we probably do return something....
  return 0;
}
int test_next_image()
{
  printf("Testing cmd 0x52, 0x81 0x55 (Load next ODE image)\n");
  uint8_t params[11] = { 0x81, 0x55, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  cdops_exec_cmd(0x52, &params);
  return 0;
}
int test_prev_image()
{
  printf("Testing cmd 0x52, 0x81 0x44 (Load prev ODE image)\n");
  uint8_t params[11] = { 0x81, 0x44, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  cdops_exec_cmd(0x52, &params);
  return 0;
}
int test_load_indexed_image()
{
  printf("Testing cmd 0x52, 0x82 0x01 (Load ODE image indexed at 1)\n");
  uint8_t params[11] = { 0x82, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  cdops_exec_cmd(0x52, &params);
  return 0;
}

void run_test()
{
  if (!select_track_manual_toc())
    return;
  
  test_get_ver();
  test_next_image();

  if (!select_track_manual_toc())
    return;
  
//  test_prev_image();

//  if (!select_track())
//    return;
//  test_load_indexed_image(0);

//  if (!select_track())
//    return;
}
