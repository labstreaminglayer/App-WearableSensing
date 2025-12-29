/*
 * dsi2lsl.c
 * ---------------------------------------------
 * Integration between Wearable Sensing DSI C/C++ API and Lab Streaming Layer (LSL).
 *
 * This program acquires data from a DSI headset and streams it over LSL for real-time
 * data acquisition and analysis. It uses Windows threads for parallel processing:
 *   - DSI headset thread: continuously calls DSI_Headset_Idle to process incoming data.
 *   - Impedance thread: controls impedance measurement driver via runtime commands.
 *
 * Usage:
 *   - Run the executable and specify options via command line (see GlobalHelp).
 *   - Data is streamed to LSL and can be received by compatible clients.
 *   - Runtime commands: checkZOn, checkZOff, resetZ
 *
 * For support or feature requests, create a GitHub Issue or contact support@wearablesensing.com.
 */

#include "DSI.h"
#include "lsl_c.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <windows.h>


// -----------------------------------------------------------------------------
// Function Prototypes and Helper Macros
// -----------------------------------------------------------------------------
int               StartUp( int argc, const char * argv[], DSI_Headset *headsetOut, int * helpOut );
int                Finish( DSI_Headset h );
int            GlobalHelp( int argc, const char * argv[] );
lsl_outlet        InitLSL( DSI_Headset h, const char * streamName);
void             OnSample( DSI_Headset h, double packetOffsetTime, void * userData);
void      getRandomString( char *s, const int len);
const char * GetStringOpt( int argc, const char * argv[], const char * keyword1, const char * keyword2 );
int         GetIntegerOpt( int argc, const char * argv[], const char * keyword1, const char * keyword2, int defaultValue );
int        startAnalogReset( DSI_Headset h );  
int          CheckImpedance( DSI_Headset h ); 
void        PrintImpedances( DSI_Headset h, double packetOffsetTime, void * userData );

// Global control flags
static volatile int KeepRunning = 1;      // Main loop control
static volatile int DSI_Thread_Paused = 0;// Pause DSI thread

/**
 * Signal handler for graceful shutdown (Ctrl+C)
 */
void QuitHandler(int a) { KeepRunning = 0; }

// Error checking macros
#define REPORT(fmt, x)  fprintf(stderr, #x " = " fmt "\n", (x))
int CheckError(void) {
  if (DSI_Error()) return fprintf(stderr, "%s\n", DSI_ClearError());
  else return 0;
}
#define CHECK   if (CheckError() != 0) return -1;

#define MAX_COMMAND_LENGTH 256
#define BUFFER_MILLISECONDS 1 // Sleep time for thread scheduling (milliseconds)
#define THREAD_INIT_WAIT_MS 500 // Wait time for thread initialization (milliseconds)
#define SHUTDOWN_IDLE_TIMEOUT 2.0 // Timeout for final packet reception during shutdown (seconds)
#define DEFAULT_ACCEL_RATE 30 // Default accelerometer sampling rate in Hz

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
/**
 * CHUNK_SIZE: Number of samples per chunk pushed to LSL.
 * Increasing CHUNK_SIZE beyond 9 increases timestamp difference.
 * Decreasing it increases jitter.
 */
#define CHUNK_SIZE 9

// -----------------------------------------------------------------------------
// Thread Parameter Structs
// -----------------------------------------------------------------------------
/**
 * ThreadParams: Parameters for impedance thread control.
 */
typedef struct {
  DSI_Headset h;           // Headset handle
  volatile int printFlag;  // Print impedance flag
  volatile int startFlag;  // Start impedance flag
  volatile int stopFlag;   // Stop impedance flag
  lsl_outlet outlet;       // LSL outlet
} ThreadParams;

/**
 * DSI_Processing_Thread
 * ---------------------
 * Thread function to continuously call DSI_Headset_Idle for data processing.
 * @param lpParam: Pointer to DSI_Headset
 * @return DWORD: 0 on success
 */
DWORD WINAPI DSI_Processing_Thread(LPVOID lpParam) {
    DSI_Headset h = (DSI_Headset)lpParam;
    fprintf(stdout, "DSI processing thread started.\n");

    while (KeepRunning == 1) {
        /* Only call Idle if the main thread hasn't paused us. */
        if (!DSI_Thread_Paused) {
            /* Process data as quickly as possible with zero timeout */
            DSI_Headset_Idle(h, 0.0);
            if (CheckError() != 0) {
                fprintf(stderr, "Error in DSI processing thread. Exiting.\n");
                KeepRunning = 0; /* Signal main thread to exit. */
            }
        }
        else {
            /* If paused, sleep to avoid busy-waiting */
            Sleep(BUFFER_MILLISECONDS);
        }
    }

    fprintf(stdout, "DSI processing thread finished.\n");
    return 0;
}

/**
 * ImpedanceThread
 * ---------------
 * Thread function to check for impedance activity and control impedance driver.
 * @param lpParam: Pointer to ThreadParams
 * @return DWORD: 0 on success
 */
DWORD WINAPI ImpedanceThread(LPVOID lpParam) {
    fprintf(stdout, "DSI impedance thread started.\n");
    ThreadParams *params = (ThreadParams *)lpParam;
    DSI_Headset h = params->h;

    while(KeepRunning == 1){
      if(params->startFlag){
        DSI_Headset_StartImpedanceDriver( h ); CHECK
        // PrintImpedances( h, 0, "headings"); CHECK 
        /* Switch OnSample to PrintImpedances to print impedance values instead of raw signals. */
        DSI_Headset_SetSampleCallback( h, OnSample, params->outlet ); CHECK
        params->startFlag = 0;
      }
      /* Uncomment the following lines to continuously print impedance check. */
      // while (params->printFlag) {
      //     DSI_Headset_Receive( h, 0.1, 0 ); CHECK
      // }
      if(params->stopFlag){
        DSI_Headset_StopImpedanceDriver( h ); CHECK
        DSI_Headset_SetSampleCallback( h, OnSample, params->outlet ); CHECK
        params->stopFlag = 0;
      }
      
      if (CheckError() != 0) {
          fprintf(stderr, "Error in impedance thread. Exiting.\n");
          KeepRunning = 0; /* Signal main thread to exit. */
      }
    }
    
    fprintf(stdout, "DSI impedance thread finished.\n");
    return 0;
}

/**
 * main
 * ----
 * Entry point. Initializes DSI API, headset, LSL outlet, and starts threads.
 */
int main(int argc, const char *argv[])
{
  srand((unsigned int)time(NULL)); // Seed RNG
  const char *dllname = NULL;
  char command[MAX_COMMAND_LENGTH];
  HANDLE sThread, iThread;

  // Load DSI DLL
  int load_error = Load_DSI_API(dllname);
  if (load_error < 0) return fprintf(stderr, "failed to load dynamic library \"%s\"\n", DSI_DYLIB_NAME(dllname));
  if (load_error > 0) return fprintf(stderr, "failed to import %d functions from dynamic library \"%s\"\n", load_error, DSI_DYLIB_NAME(dllname));
  fprintf(stderr, "DSI API version %s loaded\n", DSI_GetAPIVersion());
  if (strcmp(DSI_GetAPIVersion(), DSI_API_VERSION) != 0)
    fprintf(stderr, "WARNING - mismatched versioning: program was compiled with DSI.h version %s but just loaded shared library version %s. You should ensure that you are using matching versions of the API files - contact Wearable Sensing if you are missing a file.\n", DSI_API_VERSION, DSI_GetAPIVersion());

  // Set up Ctrl+C handler
  signal(SIGINT, QuitHandler);

  // Initialize API and headset
  DSI_Headset h;
  int help, error;
  error = StartUp(argc, argv, &h, &help);
  if (error || help) {
    GlobalHelp(argc, argv);
    return error;
  }

  // Initialize LSL outlet
  const char *streamName = GetStringOpt(argc, argv, "lsl-stream-name", "m");
  if (!streamName) streamName = "WS-default";
  fprintf(stdout, "Initializing %s outlet\n", streamName);
  lsl_outlet outlet = InitLSL(h, streamName); CHECK;

  /* Set the sample callback (forward every data sample received to LSL) */
  DSI_Headset_SetSampleCallback( h, OnSample, outlet ); CHECK

  /* Start data acquisition */
  fprintf(stdout, "Starting data acquisition\n");
  DSI_Headset_StartDataAcquisition( h ); CHECK

  /* Custom struct for impedance flags */
  ThreadParams zFLag;
  zFLag.h = h; /* Valid DSI_Headset variable */
  zFLag.printFlag = 0; /* Used to print impedance continuously */
  zFLag.startFlag = 0;
  zFLag.stopFlag = 0;
  zFLag.outlet = outlet; /* Valid LSL outlet */

  /* Create the impedance thread */
  iThread = CreateThread(NULL, 0, ImpedanceThread, &zFLag, 0, NULL);
  if (iThread == NULL) {
      fprintf(stderr, "Error creating DSI impedance thread.\n");
      return Finish(h);
  }
   /* Create and start the DSI processing thread */
  sThread = CreateThread(NULL, 0, DSI_Processing_Thread, h, 0, NULL);
  if (sThread == NULL) {
      fprintf(stderr, "Error creating DSI processing thread.\n");
      /* Close the impedance thread handle to prevent leak */
      if (iThread != NULL) {
          KeepRunning = 0;
          WaitForSingleObject(iThread, 1000);
          CloseHandle(iThread);
      }
      return Finish(h);
  }
  
  fprintf(stderr, "Wait...\n");
  Sleep(THREAD_INIT_WAIT_MS); /* Wait for threads to initialize properly */
  fprintf(stderr, "Setup Ready\n");
  /* Start streaming */
  fprintf(stdout, "Streaming...\n");
  while( KeepRunning==1 ){
    
    /* 
     * Read a line of input from stdin (the terminal)
     * fgets reads up to MAX_COMMAND_LENGTH-1 characters or until a newline.
     * It includes the newline character if it fits in the buffer. 
     */
    if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
        /* Handle potential error or EOF (End Of File) condition. */
        fprintf(stdout, "Error reading input or EOF reached.\n");
        break; /* Exit the loop on error */
    }

    /* 
     * Remove the trailing newline character if it exists
     * fgets includes the newline, which can interfere with string comparisons.
     */
    command[strcspn(command, "\n")] = 0;

    if (command[0] == '\0') {
        /* If no command was entered (just spaces or empty line after stripping newline) */
        continue; /* Go back to the prompt */
    }

    else if (strcmp(command, "checkZOn") == 0) {
        /* Start impedance driver */
        zFLag.stopFlag = 0;
        zFLag.startFlag = 1;

    }else if (strcmp(command, "checkZOff") == 0) {
        /* Stop impedance driver */
        zFLag.stopFlag = 1;
        zFLag.startFlag = 0;
    }
    else if (strcmp(command, "resetZ") == 0) {
        /* Reset impedance */
        startAnalogReset( h ); CHECK
    }
  }

  /* Closing the threads */
  if (sThread != NULL) {
      fprintf(stdout, "Waiting for DSI thread to terminate...\n");
      WaitForSingleObject(sThread, INFINITE);
      CloseHandle(sThread);
      fprintf(stdout, "DSI thread has terminated.\n");
  }
  if (iThread != NULL) {
      fprintf(stdout, "Waiting for impedance thread to terminate...\n");
      WaitForSingleObject(iThread, INFINITE);
      CloseHandle(iThread);
      fprintf(stdout, "Impedance thread has terminated.\n");
  }
  

  /* Gracefully exit the program */
  fprintf(stdout, "\n%s will exit now...\n", argv[ 0 ]);
  lsl_destroy_outlet(outlet);
  return Finish( h );
}

/**
 * Reset the analog impedance for a DSI headset.
 *
 * @param h - Valid DSI headset handle
 * @return 0 on success
 */
int startAnalogReset(DSI_Headset h) {
    if (h == NULL) {
        fprintf(stderr, "Error: Invalid headset handle.\n");
        return -1;
    }
    /* Check initial analog reset mode */
    fprintf(stdout, "--> Initial analog reset mode: %d\n", DSI_Headset_GetAnalogResetMode(h));

    DSI_Headset_StartAnalogReset(h); CHECK
    return 0;
}

/**
 * Callback function invoked for each sample received from the DSI headset.
 * Collects samples into a chunk buffer and pushes them to the LSL outlet in batches.
 *
 * @param h - Valid DSI headset handle
 * @param unused_packet_offset_time - Unused packet offset time
 * @param outlet - LSL outlet to push data to
 */
/* Helper struct for chunk buffer management */
typedef struct {
    float* buffer;
    int sample_index_in_chunk;
    unsigned int numberOfChannels;
} ChunkBufferManager;

/* Helper function to initialize or get chunk buffer */
static ChunkBufferManager* GetChunkBufferManager(DSI_Headset h, ChunkBufferManager **manager_ptr) {
    if (*manager_ptr == NULL) {
        *manager_ptr = (ChunkBufferManager*)malloc(sizeof(ChunkBufferManager));
        if (*manager_ptr == NULL) {
            fprintf(stderr, "Fatal Error: Could not allocate memory for ChunkBufferManager.\n");
            return NULL;
        }
        (*manager_ptr)->numberOfChannels = DSI_Headset_GetNumberOfChannels(h);
        (*manager_ptr)->sample_index_in_chunk = 0;
        if ((*manager_ptr)->numberOfChannels > 0) {
            (*manager_ptr)->buffer = (float*)malloc(CHUNK_SIZE * (*manager_ptr)->numberOfChannels * sizeof(float));
            if ((*manager_ptr)->buffer == NULL) {
                fprintf(stderr, "Fatal Error: Could not allocate memory for chunk buffer.\n");
                free(*manager_ptr);
                *manager_ptr = NULL;
                return NULL;
            }
        } else {
            (*manager_ptr)->buffer = NULL;
        }
    }
    return *manager_ptr;
}

/* Helper function to free chunk buffer */
static void FreeChunkBufferManager(ChunkBufferManager **manager_ptr) {
    if (*manager_ptr) {
        if ((*manager_ptr)->buffer) free((*manager_ptr)->buffer);
        free(*manager_ptr);
        *manager_ptr = NULL;
    }
}

static ChunkBufferManager *onSampleManager = NULL;

/**
 * OnSample
 * --------
 * Callback for each sample received from DSI headset.
 * Buffers samples and pushes them to LSL in chunks.
 *
 * @param h: DSI headset handle
 * @param unused_packet_offset_time: Unused
 * @param outlet: LSL outlet
 */
void OnSample(DSI_Headset h, double unused_packet_offset_time, void *outlet)
{
  (void)unused_packet_offset_time;
  ChunkBufferManager *manager = GetChunkBufferManager(h, &onSampleManager);
  if (!manager || !manager->buffer) return;

  // Fill buffer with current sample data
  float* current_sample_ptr = &manager->buffer[manager->sample_index_in_chunk * manager->numberOfChannels];
  for (unsigned int channelIndex = 0; channelIndex < manager->numberOfChannels; channelIndex++) {
    current_sample_ptr[channelIndex] = (float)DSI_Channel_GetSignal(DSI_Headset_GetChannelByIndex(h, channelIndex));
  }

  manager->sample_index_in_chunk++;

  // Push chunk to LSL when buffer is full
  if (manager->sample_index_in_chunk == CHUNK_SIZE) {
    lsl_push_chunk_ft(outlet, manager->buffer, (size_t)(CHUNK_SIZE * manager->numberOfChannels), lsl_local_clock());
    manager->sample_index_in_chunk = 0;
  }
}

int Message( const char * msg, int debugLevel ){
  return fprintf( stderr, "DSI Message (level %d): %s\n", debugLevel, msg );
}

/**
 * Initializes and connects to the DSI headset, prepares it for
 * data acquisition.
 *
 * @param argc - Command-line argument count
 * @param argv - Command-line argument vector
 * @param headsetOut - Output pointer to store initialized headset handle.
 * @param helpOut - Output flag indicating if help was requested.
 *
 * @return 0 on success, non-zero on error.
 */
int StartUp( int argc, const char * argv[], DSI_Headset * headsetOut, int * helpOut )
{
  DSI_Headset h;

  /* Read out any configuration options. */
  int          help       = GetStringOpt(  argc, argv, "help",      "h" ) != NULL;
  const char * serialPort = GetStringOpt(  argc, argv, "port",      "p" );
  const char * montage    = GetStringOpt(  argc, argv, "montage",   "m" );
  const char * reference  = GetStringOpt(  argc, argv, "reference", "r" );
  int          verbosity  = GetIntegerOpt( argc, argv, "verbosity", "v", 2 );
  if( headsetOut ) *headsetOut = NULL;
  if( helpOut ) *helpOut = help;
  if( help ) return 0;

  /* Passing NULL defers setup of the serial port connection until later... */
  h = DSI_Headset_New( NULL ); CHECK

  /*
   * ...which allows us to configure the way we handle any debugging messages
   * that occur during connection (see our definition of the `DSI_MessageCallback`
   * function `Message()` above).
   */
  DSI_Headset_SetMessageCallback( h, Message ); CHECK
  DSI_Headset_SetVerbosity( h, verbosity ); CHECK

  /*
   * Now we establish the serial port connection and initialize the headset.
   * In this demo program, the string supplied in the --port command-line
   * option is used as the serial port address (if this string is empty, the
   * API will automatically look for an environment variable called
   * DSISerialPort).
   */
  DSI_Headset_Connect( h, serialPort ); CHECK

  /*
   * Sets up the montage according to strings supplied in the --montage and
   * --reference command-line options, if any.
   */
  DSI_Headset_ChooseChannels( h, montage, reference, 1 ); CHECK

  /*
   * Check accelerometer feature availability and configure if enabled.
   * The info string contains FeatureAvailability with "Accelerometer":1 (double quotes)
   * and AccelerometerRate with 'AccelerometerRate': 30 (single quotes).
   */
  const char *infoString = DSI_Headset_GetInfoString(h);
  if (infoString) {
    /* Look for "Accelerometer" in FeatureAvailability (uses double quotes in JSON object) */
    const char *accelField = strstr(infoString, "\"Accelerometer\"");
    if (accelField) {
      /* Move past the field name to find the value after the colon */
      accelField = strchr(accelField, ':');
      if (accelField) {
        accelField++; /* Move past the colon */
        /* Skip whitespace */
        while (*accelField == ' ' || *accelField == '\t') accelField++;
        /* Check if value is 1 (enabled) */
        if (*accelField == '1') {
          /* Check current accelerometer rate from info string (uses single quotes) */
          const char *rateField = strstr(infoString, "'AccelerometerRate'");
          int currentRate = 0;
          if (rateField) {
            rateField = strchr(rateField, ':');
            if (rateField) {
              rateField++; /* Move past the colon */
              /* Skip whitespace */
              while (*rateField == ' ' || *rateField == '\t') rateField++;
              {
                char *endptr = NULL;
                long parsedRate = strtol(rateField, &endptr, 10);
                if (endptr != rateField) {
                  currentRate = (int)parsedRate;
                } else {
                  /* Parsing failed; keep currentRate as 0 to trigger default behavior */
                  currentRate = 0;
                }
              }
            }
          }
          
          if (currentRate == 0) {
            fprintf(stderr, "Accelerometer is enabled but rate is 0. Setting rate to %d Hz\n", DEFAULT_ACCEL_RATE);
            DSI_Headset_SetAccelerometerRate(h, DEFAULT_ACCEL_RATE); CHECK
          } else {
            fprintf(stderr, "Accelerometer is already enabled at %d Hz\n", currentRate);
          }
        }
      }
    }
  }

  /* Prints an overview of what is known about the headset. */
  fprintf( stderr, "%s\n", DSI_Headset_GetInfoString( h ) ); CHECK


  if( headsetOut ) *headsetOut = h;
  if( helpOut ) *helpOut = help;
  return 0;
}

static ChunkBufferManager *impedanceManager = NULL;

int Finish( DSI_Headset h )
{
  /* This stops our application from responding to received samples. */
  DSI_Headset_SetSampleCallback( h, NULL, NULL ); CHECK

  /* Free the buffer allocated in OnSample and PrintImpedances to prevent memory leak. */
  FreeChunkBufferManager(&onSampleManager);
  FreeChunkBufferManager(&impedanceManager);

  /* This sends a command to the headset to stop sending samples. */
  DSI_Headset_StopDataAcquisition( h ); CHECK

  /*
   * This allows more than enough time to receive any samples that were
   * sent before the stop command is carried out, along with the alarm
   * signal that the headset sends out when it stops.
   */
  DSI_Headset_Idle( h, SHUTDOWN_IDLE_TIMEOUT ); CHECK

  /*
   * This is the only really necessary step. Disconnects from the serial
   * port, frees memory, etc.
   */
  DSI_Headset_Delete( h ); CHECK

  return 0;
}

void getRandomString(char *s, const int len)
{
  int i = 0;
  static const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)rand();
  srand(seed); // Reseed for more randomness per call
  for (i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  s[len] = 0;
}

lsl_outlet InitLSL(DSI_Headset h, const char * streamName)
{
  unsigned int channelIndex;
  unsigned int numberOfChannels = DSI_Headset_GetNumberOfChannels( h );
  double samplingRate = DSI_Headset_GetSamplingRate( h );

	/* Output stream declaration object */
  lsl_streaminfo info;
	/* Some xml element pointers */
  lsl_xml_ptr desc, chn, chns, ref; 
  #define IMAX 16
  char source_id[IMAX];
  char *long_label;
  char *short_label;
  char *reference;
	
	/* Note: an even better choice here may be the serial number of the device. */
  getRandomString(source_id, IMAX);
  fprintf(stderr, "Source ID: %s\n", source_id);

  /* Declare a new streaminfo (name: WearableSensing, content type: EEG, number of channels, srate, float values, source id. */
  info = lsl_create_streaminfo((char*)streamName,"EEG",numberOfChannels,samplingRate,cft_float32,source_id);

  if(!info) {
      fprintf(stderr, "Failed to create LSL streaminfo.\n");
      return NULL;
  }
  fprintf(stderr, "Stream Name: %s\n", streamName);
  /* Add some meta-data fields to it (for more standard fields, see https://github.com/sccn/xdf/wiki/Meta-Data). */
  desc = lsl_get_desc(info);
  lsl_append_child_value(desc,"manufacturer","WearableSensing");

	/* Describe channel info */
  chns = lsl_append_child(desc,"channels");
  for( channelIndex=0; channelIndex < numberOfChannels ; channelIndex++)
  {
    chn = lsl_append_child(chns,"channel");

    long_label = (char*) DSI_Channel_GetString( DSI_Headset_GetChannelByIndex( h, channelIndex ) );
    /* Cut off "negative" part of channel name (e.g., the ref chn) */
    char label_buffer[256];
    strncpy_s(label_buffer, sizeof(label_buffer), long_label, _TRUNCATE);
    label_buffer[sizeof(label_buffer) - 1] = '\0';
    char *context = NULL;
    short_label = strtok_s(label_buffer, "-", &context);
    if(short_label == NULL)
      short_label = label_buffer;
    /* Commit channel info to LSL stream */
    lsl_append_child_value(chn,"label", short_label);
    lsl_append_child_value(chn,"unit","microvolts");
    lsl_append_child_value(chn,"type","EEG");
  }
	
	/* Describe reference used */
  reference = (char*)DSI_Headset_GetReferenceString(h);
  ref = lsl_append_child(desc,"reference");
  lsl_append_child_value(ref,"label", reference);
  fprintf(stdout, "REF: %s\n", reference);

  /* Make a new outlet (chunking: default, buffering: 360 seconds). */
  lsl_outlet outlet = lsl_create_outlet(info, 0, 360);
  
  /* Free streaminfo as it's no longer needed after creating the outlet */
  lsl_destroy_streaminfo(info);
  
  return outlet;
}


int GlobalHelp( int argc, const char * argv[] )
{
  fprintf( stderr,
            "Usage: %s [ --OPTIONS... ]\n\n"
            "With the exception of --help,\n"
            "the options should be given in --NAME=VALUE format.\n"
            "\n"
            "  --help\n"
            "       Displays this help text.\n"
            "\n"
            "  --port\n"
            "       Specifies the serial port address (e.g. --port=COM4 on Windows,\n"
            "       --port=/dev/cu.DSI24-023-BluetoothSeri on OSX, or --port=/dev/rfcomm0 on Linux) on which to connect.\n"
            "       Note: if you omit this option, or use an empty string or the string\n"
            "       \"default\", then the API will look for an environment variable called\n"
            "       DSISerialPort and use the content of that, if available.\n"
            "\n"
            "  --montage\n"
            "       A list of channel specifications, comma-separated without spaces,\n"
            "       (can also be space-delimited, but then you would need to enclose the\n"
            "       option in quotes on the command-line).\n"
            "\n"
            "  --reference\n"
            "       The name of sensor (or linear combination of sensors, without spaces)\n"
            "       to be used as reference. Defaults to a \"traditional\" averaged-ears or\n"
            "       averaged-mastoids reference if available, or the factory reference\n"
            "       (typically Pz) if these sensors are not available.\n"
            "\n"
            "  --verbosity\n"
            "       The higher the number, the more messages the headset will send to the\n"
            "       registered `DSI_MessageCallback` function, and hence to the console\n"
            "       (and the more low-level they will tend to be)\n"
            "\n"
            "  --lsl-stream-name\n"
            "       The name of the LSL outlet that will be created to stream the samples\n"
            "       received from the device. If omitted, the stream will be given the name WS-default.\n"
            "\n"
        , argv[ 0 ] );
        return 0;
}


/*
 * These functions are carried over from the Wearable Sensing example code 
 * and are Copyright (c) 2014-2025 Wearable Sensing LLC.
 *
 * Helper function for figuring out command-line input flags like --port=COM4
 * or /port:COM4 (either syntax will work).  Returns NULL if the specified
 * option is absent. Returns a pointer to the argument value if the option
 * is present (the pointer will point to '\0' if the argument value is empty
 * or not supplied as part of the option string). 
 */
const char * GetStringOpt( int argc, const char * argv[], const char * keyword1, const char * keyword2 )
{
    int i, j;
    const char * result = NULL;
    const char * keyword;
    for( i = 1; i < argc; i++ )
    {
        int isopt = 0;
        const char * arg = argv[ i ];
        if( !arg ) continue;
        for( j = 0; arg[ j ]; j++ ) isopt |= arg[ j ] == '-' || arg[ j ] == '=' || arg[ j ] == '/' || arg[ j ] == ':';
        if( *arg == '-' || *arg == '/' ) ++arg;
        if( *arg == '-' || *arg == '/' ) ++arg;
        for( j = 0, keyword = keyword1; j < 2; j++, keyword = keyword2  )
        {
            if( keyword && strncmp( arg, keyword, strlen( keyword ) ) == 0 )
            {
                const char * potential = arg + strlen( keyword );
                if( *potential == '=' || *potential == ':' ) result = potential + 1;
                else if( *potential == '\0' || ( *keyword == '\0' && !isopt ) ) result = potential;
            }
        }
    }
    return result;
}

int GetIntegerOpt( int argc, const char * argv[], const char * keyword1, const char * keyword2, int defaultValue )
{
    char * end;
    int result;
    const char * stringValue = GetStringOpt( argc, argv, keyword1, keyword2 );
    if( !stringValue || !*stringValue ) return defaultValue;
    result = (int)strtol( stringValue, &end, 10 );
    return result;
}

/**
 * PrintImpedances
 * ---------------
 * Callback function to collect impedance values from the DSI headset and push them to the LSL outlet in chunks.
 *
 * @param h                Valid DSI headset handle.
 * @param packetOffsetTime Unused packet offset time.
 * @param outlet           LSL outlet to push impedance data to.
 *
 * Buffers impedance values for each channel and pushes them to LSL when the buffer is full.
 */
void PrintImpedances( DSI_Headset h, double packetOffsetTime, void * outlet )
{
    (void)packetOffsetTime;
    ChunkBufferManager *manager = GetChunkBufferManager(h, &impedanceManager);
    if (!manager || !manager->buffer) return;

    float* current_sample_ptr = &manager->buffer[manager->sample_index_in_chunk * manager->numberOfChannels];

    for (unsigned int channelIndex = 0; channelIndex < manager->numberOfChannels; channelIndex++) {
        current_sample_ptr[channelIndex] = (float) DSI_Source_GetImpedanceEEG(
            DSI_Headset_GetSourceByIndex(h, channelIndex));
    }

    manager->sample_index_in_chunk++;

    if (manager->sample_index_in_chunk == CHUNK_SIZE) {
        lsl_push_chunk_ft(outlet, manager->buffer, (size_t)(CHUNK_SIZE * manager->numberOfChannels), lsl_local_clock());
        manager->sample_index_in_chunk = 0;
    }
}
