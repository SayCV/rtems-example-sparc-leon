#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <rtems.h>

#include "vt100.h"

#define VT100_CURSOR_POS(row,col) fprintf(stream, ESC "[%d;%dH",row,col)
#define VT100_GET_POS fprintf(stream, ESC "[6n")
#define VT100_SCROLL_MARGIN(top_row,bot_row) fprintf(stream,ESC "[%d;%dr",top_row,bot_row)

#define FLUSH fflush(stream)

static FILE *stream;
static rtems_id vt100_lock;

void vt100_clear(void){
	/*printf(VT100_CLEAR_SCREEN);*/
	fprintf(stream,VT100_CLR);
}

void vt100_pos(int row, int col){
	VT100_CURSOR_POS(row,col);
}

void vt100_home(void)
{
  fprintf(stream,VT100_HOME);
}

void vt100_flush(void)
{
  FLUSH;
}


void vt100_scroll_window(int top, int bot){
	VT100_SCROLL_MARGIN(top,bot);
}
void vt100_reset(void){
  fprintf(stream,VT100_RESET);
	vt100_clear();
	FLUSH;
}

int vt100_init(FILE *out_stream){
  if ( !out_stream ){
    stream = stdout;
  }else{
    stream = out_stream;
  }

  rtems_semaphore_create(
		rtems_build_name('V', 'T', 'L', 'K'),
		1,
		RTEMS_FIFO | RTEMS_COUNTING_SEMAPHORE | RTEMS_NO_INHERIT_PRIORITY | \
		RTEMS_NO_PRIORITY_CEILING, 
		0,
		&(vt100_lock));
    
 	vt100_reset();
	return 0;
}

int vt100_printf(int row, int col, char *fmt, ...){
  int ret;
	va_list va;
		
	/* Get print lock */
	rtems_semaphore_obtain(vt100_lock, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
  
  /* Set posistion */
  vt100_pos(row,col);
	
  va_start(va,fmt);
  
	/* Print string at new posistion */
  ret = vfprintf(stream,fmt,va);
	
	/* Release print lock */
	rtems_semaphore_release(vt100_lock);
  
	return ret;
}
