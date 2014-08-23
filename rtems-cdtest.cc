/*
 *  This routine is the initialization task for this test program.
 *  It is called from init_exec and has the responsibility for creating
 *  and starting the tasks that make up the test.  If the time of day
 *  clock is required for the test, it should also be set to a known
 *  value by this function.
 *
 *  Input parameters:  NONE
 *
 *  Output parameters:  NONE
 *
 *  COPYRIGHT (c) 1994 by Division Incorporated
 *  Based in part on OAR works.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.OARcorp.com/rtems/license.html.
 *
 *
 *  by Rosimildo da Silva:
 *  Modified the test a bit to indicate when an instance is
 *  global or not, and added code to test C++ exception.
 *
 *
 *  main.cc,v 1.13 2001/11/26 14:33:43 joel Exp
 */

#include <rtems.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef RTEMS_TEST_IO_STREAM
#include <iostream.h>
#endif

extern "C" 
{

#define CONFIGURE_INIT

#include <rtems.h>

/* functions */

rtems_task main_task(
  rtems_task_argument argument
);

/* configuration information */

#include <bsp.h> /* for device driver prototypes */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_MAXIMUM_TASKS           1
#define CONFIGURE_MAXIMUM_SEMAPHORES      1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT_TASK_ENTRY_POINT   main_task
#define CONFIGURE_INIT_TASK_NAME          rtems_build_name( 'C', 'T', 'O', 'R' )

#include <rtems/confdefs.h>

/* If --drvmgr was enabled during the configuration of the RTEMS kernel */
#ifdef RTEMS_DRVMGR_STARTUP
 #ifdef LEON3
  /* Add Timer and UART Driver for this example */
  #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
  #endif
  #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
   #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
  #endif
 #endif
 #include <drvmgr/drvmgr_confdefs.h>
#endif

#include <stdio.h>
static inline uint32_t get_ticks_per_second( void )
{
  rtems_interval ticks_per_second;
  (void) rtems_clock_get( RTEMS_CLOCK_GET_TICKS_PER_SECOND, &ticks_per_second );
  return ticks_per_second;
}

extern rtems_task main_task(rtems_task_argument);
}

static int num_inst = 0;

class AClass {
public:
  AClass(const char *p = "LOCAL" ) : ptr( p )
    {
        num_inst++;
        printf(
          "%s: Hey I'm in base class constructor number %d for %p.\n",
          p, num_inst, this
        );

	/*
	 * Make sure we use some space
	 */

        string = new char[50];
	sprintf(string, "Instantiation order %d", num_inst);
    };

    virtual ~AClass()
    {
    	// MUST USE PRINTK -- RTEMS IS SHUTTING DOWN WHEN THIS RUNS
        printk(
          "%s: Hey I'm in base class destructor number %d for %p.\n",
          ptr, num_inst, this
        );
        printk("Derived class - %s\n", string);
        num_inst--;
    };

    virtual void print()  { printf("%s\n", string); };

protected:
    char  *string;
    const char *ptr;
};

class BClass : public AClass {
public:
  BClass(const char *p = "LOCAL" ) : AClass( p ) 
    {
        num_inst++;
        printf(
          "%s: Hey I'm in derived class constructor number %d for %p.\n",
          p, num_inst,  this
        );

	/*
	 * Make sure we use some space
	 */

        string = new char[50];
	sprintf(string, "Instantiation order %d", num_inst);
    };

    ~BClass()
    {
        printk(
          "%s: Hey I'm in derived class destructor number %d for %p.\n",
          ptr, num_inst,
          this
        );
        printk("Derived class - %s\n", string);
        num_inst--;
    };

    void print()  { printf("Derived class - %s\n", string); }
};


class RtemsException 
{
public:
    
    RtemsException( char *module, int ln, int err = 0 )
    : error( err ), line( ln ), file( module )
    {
      printf( "RtemsException raised=File:%s, Line:%d, Error=%X\n",
               file, line, error ); 
    }

    void show()
    {
      printf( "RtemsException ---> File:%s, Line:%d, Error=%X\n",
               file, line, error ); 
    }

private:
   int  error;
   int  line;
   char *file;

};



AClass foo( "GLOBAL" );
BClass foobar( "GLOBAL" );

void
cdtest(void)
{
    AClass bar, blech, blah;
    BClass bleak;

#ifdef RTEMS_TEST_IO_STREAM
    cout << "Testing a C++ I/O stream" << endl;
#else
    printf("IO Stream not tested\n");
#endif
    bar = blech;
    rtems_task_wake_after( 5 * get_ticks_per_second() );
}

//
// main equivalent
//      It can not be called 'main' since the bsp owns that name
//      in many implementations in order to get global constructors
//      run.
//

static void foo_function()
{
    try 
    {
      throw "foo_function() throw this exception";  
    }
    catch( const char *e )
    {
     printf( "foo_function() catch block called:\n   < %s  >\n", e );
     throw "foo_function() re-throwing execption...";  
    }
}

rtems_task main_task(
  rtems_task_argument 
)
{
    printf( "\n\n*** CONSTRUCTOR/DESTRUCTOR TEST ***\n" );

    cdtest();

    printf( "*** END OF CONSTRUCTOR/DESTRUCTOR TEST ***\n\n\n" );


    printf( "*** TESTING C++ EXCEPTIONS ***\n\n" );

    try 
    {
      foo_function();
    }
    catch( const char *e )
    {
       printf( "Success catching a char * exception\n%s\n", e );
    }
    try 
    {
      printf( "throw an instance based exception\n" );
		throw RtemsException( __FILE__, __LINE__, 0x55 ); 
    }
    catch( RtemsException & ex ) 
    {
       printf( "Success catching RtemsException...\n" );
       ex.show();
    }
    catch(...) 
    {
      printf( "Caught another exception.\n" );
    }
    printf( "Exceptions are working properly.\n" );
    rtems_task_wake_after( 5 * get_ticks_per_second() );
    printf( "Global Dtors should be called after this line....\n" );
    exit(0);
}
