/********************************************************************\

  Name:         feAttenuator.c 
  Created by:   Thomas Lindner
  Code based on work from Jerin Roberts 
  Sept 2015

  Contents:    Set the OZ optics digital attenuation through RS-232


\********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "midas.h"
#include "msystem.h"
#include "mcstd.h"
#include <math.h>
#include <unistd.h>
#include <string.h>

/* make frontend functions callable from the C framework            */
#ifdef __cplusplus
extern "C" {
#endif

/*-- Globals -------------------------------------------------------*/



/* The frontend name (client name) as seen by other MIDAS clients   */
char *frontend_name = "feAttenuator";

/* The frontend file name, don't change it                          */
char *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
BOOL frontend_call_loop = FALSE;

/* a frontend status page is displayed with this frequency in ms    */
//INT display_period = 3000;
INT display_period = 0;

/* maximum event size produced by this frontend                     */
INT max_event_size = 3000;

/* buffer size to hold events                                       */
INT event_buffer_size = 10*3000;

/* maximum event size for fragmented events (EQ_FRAGMENTED)         */
INT max_event_size_frag = 5*300*300;

/*-- Info structure declaration ------------------------------------*/

/*-- Function declarations -----------------------------------------*/

// Required functions
INT frontend_init();
INT frontend_exit();
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();
INT poll_trigger_event(INT count, PTYPE test);
INT interrupt_configure(INT cmd, PTYPE adr);
INT read_trigger_event(char *pevent, INT off);


/*-- Equipment list ------------------------------------------------*/

//#define USE_INT 1

EQUIPMENT equipment[] = {

  {"Attenuator",       	    // equipment name 
  {89, 0,              // event ID, trigger mask 
  "SYSTEM",           // event buffer 
  EQ_PERIODIC,        // equipment type 
  0,                  // event source 
  "MIDAS",            // format 
  TRUE,               // enabled 
  RO_ALWAYS,		      // read x 
  5000,               // read every x millisec 
  0,                  // stop run after this event limit 
  0,                  // number of sub event 
  60,                	// log history every x sec 
  "", "", "",},
  read_trigger_event, // readout routine 
  NULL,               // class driver main routine 
  NULL,        	      // device driver list 
  NULL,               // init string 
  },

  { "" }
};


  INT poll_event(INT source, INT count, BOOL test)
  /* Polling routine for events. Returns TRUE if event
     is available. If test equals TRUE, don't return. The test
     flag is used to time the polling */
  {
    return FALSE;
  }
  
#ifdef __cplusplus
}
#endif

// Handle to ODB key
HNDLE hKeyAttenuation;

/********************************************************************\
             USED callback routines for system transitions

  frontend_init:  When the frontend program is started. This routine
                  sets up all the ODB variables and hotlink for the 
                  system. The memory for pInfo is allocated here. 
  
  frontend_exit:  When the frontend program is shut down. Can be used
                  to release any locked resources like memory, commu-
                  nications ports etc.
\********************************************************************/

HNDLE   hDB=0;
#include <iostream>
#include <string>
#include "rs232.h"
using namespace std;

// Should have made serial connection into a class... maybe later...

// Make sure we don't have two connections to serial port at same time.
bool serialAlreadyOpen = false;
int serialPortNumber = 16;


// Read the serial port. Returns the read-back attenuation value.
double ReadComportAttenuation(){ 
  int n=0;

  unsigned char buf[6000]; 
  int size = sizeof(buf);


  int max_poll = 0;
  while(1) {    
    n = RS232_PollComport(serialPortNumber, buf, size);    
    if(n > 0) {
      break;
    } 
    max_poll++;

    if(max_poll > 1000000){
      cm_msg(MERROR,"ReadComportAttenuation","Failed to finished pollComport.\n");    
    
      printf("Failed to finished pollComport.\n"); break;}
  }
  
  // parse the return string to look for the attenuation value
  if(n>0){
    buf[n] = 0; /* always put a "null" at the end of a string! */
    printf("%s\n", (char *)buf);  

    // First line is 
    int i = 0;
    bool foundEOL = false;
    while(i < n and !foundEOL){      
      if(buf[i] == '\r') foundEOL = true; 
      i++;
    }
    if(foundEOL && i < n){
      // Loop through string looking for the attenuation value
      string str_att;
      for(int j = i+7; j < i+12; j++){
	//if(buf[j] == '\r') printf("yeah!");
	//printf("%c", buf[j]);       
	str_att += buf[j];
      }
      double readback_atten = atof(str_att.c_str());

      printf("The read-back value of attenuator is %5.2f\n",readback_atten);
      return readback_atten;
    }
  }


  return -1.0; 
}

// open serial connection to attenuator... check that connection
// isn't already open
int OpenAttenuatorSerialPort(){
  
  // Check if serial already open.
  if(serialAlreadyOpen){
    // if so, wait.
    for(int i = 0; i < 10; i++){
      sleep(1);
      if(!serialAlreadyOpen) break;
    }

    // Return error if port wasn't closed properly.
    if(serialAlreadyOpen){
      cm_msg(MERROR,"OpenAttenuatorSerialPort","Port not properly closed.\n");    
      return 1;
    }

  }

  serialAlreadyOpen = true;

  // Now open serial port
  int i=0,        /* /dev/ttyusb0*///see rs232.c for index
    bdrate=9600;      /* 9600 baud */
  
  char mode[]={'8','N','1',0};
  if(RS232_OpenComport(serialPortNumber, bdrate, mode))
    {
      cm_msg(MERROR,"OpenAttenuatorSerialPort","Could not open comport.\n");    
      printf("Can not open comport\n");
      
      return(1);
    }


  return 0;
}

int CloseAttenuatorSerialPort(){

  RS232_CloseComport(serialPortNumber);
  serialAlreadyOpen = false;
  return 0;
}

// Function reads the current value of attenuation from ODB,
// writes it to attenuator and checks that attenuator read-back is correct.
INT set_attenuation_rs232(){

  double value;
  int size = sizeof(double);
  char variable_name[100];
  int status;

  // Get current value of attenuation from ODB
  sprintf(variable_name,"/Equipment/Attenuator/Settings/Attenuation");
  status = db_get_value(hDB, 0, variable_name, &value, &size, TID_DOUBLE, FALSE);

  if(value < 0.0 || value >= 40.0){    
    cm_msg(MERROR,"set_attenuation_rs232","Invalid attenuation value = %f",value);    
    return 99;
  }
 

  // Now actually set the attenuator

  // I hate C string manipulation...
  char str[512];
  const char * c;
  string lasdb = "A";
  char tmp[100];
  sprintf(tmp,"%4.2f",value);
  lasdb.append(tmp);
  lasdb.append("\r");
  c = lasdb.c_str();  //string to const char*
      
  strcpy(str, c);
  
  // open serial port and send command
  if(OpenAttenuatorSerialPort()){
    cm_msg(MERROR,"set_attenuation_rs232","Error opening serial port to attenuator");
    return 99;
  }
  
  RS232_cputs(serialPortNumber, str);
  
  printf("sent: %s\n", str);

  //allow attenuator to reach correct state before continuing
  sleep(4);

  // Read-back the output...
  double readback_atten = ReadComportAttenuation(); 
  
  if(fabs(readback_atten-value) > 0.001){
    cm_msg(MERROR,"set_attenuation_rs232","Read-back attenuation (%5.2f) is different from attenuation we tried to set (%5.2f)",readback_atten,value);

  }

  CloseAttenuatorSerialPort();

  cm_msg(MINFO,"frontend_init","feAttenuator: attenuation set to %f\n",value);
  return 0;

}

void set_attenuation_rs232_callback(HNDLE hDB,HNDLE hKey,void *data){

  set_attenuation_rs232();
}



/*-- Frontend Init -------------------------------------------------*/
// Initialize ODB variables and set up hotlinks in this function
INT frontend_init()
{


  // Initialize connection to ODB.  Get the serial number for phidget.
  char    expt_name[32];
  INT status, serial_number, size;
  
  size = sizeof(expt_name);
  status = db_get_value(hDB, 0, "/experiment/Name", &expt_name, &size, TID_STRING, FALSE);

  
  /* get basic handle for experiment ODB */
  status=cm_get_experiment_database(&hDB, NULL);
  if(status != CM_SUCCESS)
    {
    cm_msg(MERROR,"frontend_init","Not connected to experiment");
    return CM_UNDEF_EXP;
  }


  /* check for serial_number */
  double attenuator_value;
  size = sizeof(attenuator_value);
  
  char variable_name[100];
  sprintf(variable_name,"/Equipment/Attenuator/Settings/Attenuation");

  status = db_get_value(hDB, 0, variable_name, &attenuator_value, &size, TID_DOUBLE, FALSE);
  if(status != DB_SUCCESS)
  {
    cm_msg(MERROR,"frontend_init","cannot GET /Equipment/Attenuator/Settings/Attenuation");
    return FE_ERR_ODB;
  }

  db_find_key(hDB,0,"/Equipment/Attenuator/Settings/Attenuation",&hKeyAttenuation);

  // Setup hotlink
  db_open_record(hDB,hKeyAttenuation,NULL,sizeof(double),MODE_READ,set_attenuation_rs232_callback,NULL);

 set_attenuation_rs232();

   return CM_SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/
// Stop the motors and free all memory in this function
INT frontend_exit()
{
	return CM_SUCCESS;
}

  


/********************************************************************\
             callback routines for system transitions

  These routines are called whenever a system transition like start/
  stop of a run occurs. The routines are called on the following
  occations:
  
  *Note: Since we do not use run control with our system, none of 
         these routines are being used in our code.

  %begin_of_run:   When a new run is started. Clear scalers, open
                   rungates, etc.

  %end_of_run:     Called on a request to stop a run. Can send 
                   end-of-run event and close run gates.

  %pause_run:      When a run is paused. Should disable trigger events.

  %resume_run:     When a run is resumed. Should enable trigger events.

\********************************************************************/

/*-- Begin of Run --------------------------------------------------*/

INT begin_of_run(INT run_number, char *error)
{
  return CM_SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  return CM_SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
  return CM_SUCCESS;
}

/*-- Resume Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{
  return CM_SUCCESS;
}

/*-- Frontend Loop -------------------------------------------------*/

INT frontend_loop()
{
  return SUCCESS;
}

/*-- Trigger event routines ----------------------------------------*/


/*-- Interrupt configuration for trigger event ---------------------*/

INT interrupt_configure(INT cmd, PTYPE adr)
{
  return CM_SUCCESS;
}

int number_of_read_failures = 0;
int max_failures = 5;

double lastReadBackValue = 0;
bool lastReadbackBad = false;

/*-- Event readout -------------------------------------------------*/
INT read_trigger_event(char *pevent, INT off)
{

  // Read the current value of attenuation
  
  // open serial port and send command
  if(OpenAttenuatorSerialPort()){
    cm_msg(MERROR,"read_trigger_event","Error opening serial port to attenuator");
    return 0;
  }
  string lasdb = "A?\r";
  char str[512];
  const char * c;
     
  //  strcpy(str, lasdb.c_str());
  string lasdb2;
  lasdb2 = "A?\r";
  
  c = lasdb2.c_str();  //string to const char*
  strcpy(str, c);


  printf("sent %s\n",str);
  RS232_cputs(serialPortNumber, str);
  usleep(100);
  sleep(1);

  // Read-back the output...
  double readback_atten = ReadComportAttenuation(); 
  
  if(readback_atten < 0 || readback_atten > 40){
    
    // Only print messages, count errors if the last one was bad.
    if(lastReadbackBad){
      cm_msg(MERROR,"read_trigger_event","Read-back attenuation (%5.2f) is not sensible",readback_atten);
      number_of_read_failures++;
      if(number_of_read_failures >= max_failures){
	cm_msg(MERROR,"read_trigger_event","Too many Read-back failures (%i). Exiting.",max_failures);
	exit(1);
      }
    }

    lastReadbackBad = true;
    CloseAttenuatorSerialPort();
    return 0;
  }

  // Print message if last readback was bad.
  if(lastReadbackBad){
    cm_msg(MINFO,"read_trigger_event","Last read-back attenuation (%5.2f) was not sensible; but this readback was fine.",readback_atten);
  }

  lastReadbackBad = false;

  CloseAttenuatorSerialPort();


  // Create a bank with the stored information from phidget
  bk_init32(pevent);

  double *pdata32;

  char bank_name[100];
  sprintf(bank_name,"ATTN");

  bk_create(pevent, bank_name, TID_DOUBLE, &pdata32);
  *pdata32++ = readback_atten;
   
  bk_close(pevent, pdata32);


  return bk_size(pevent);
}


  
