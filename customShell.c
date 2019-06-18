 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <getopt.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <errno.h>
 #include <string.h>
 #include <sys/types.h>
 #include <stdbool.h>
 #include <signal.h>
 #include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
 int trivalOptions = 0;
 int numOfFilesOpened = 0;
 int exitStat = 0;
 int pipeArray[2];
 char *segError = NULL;
 //trival options
 #define APPEND      O_APPEND
 #define CLOEXEC     O_CLOEXEC
 #define CREAT       O_CREAT
 #define DIRECTORY   O_DIRECTORY
 #define DSYNC       O_DSYNC
 #define EXCL        O_EXCL
 #define NOFOLLOW    O_NOFOLLOW
 #define NONBLOCK    O_NONBLOCK
 #define SYNC        O_SYNC
 //#define RSYNC       O_RSYNC
 #define TRUNC       O_TRUNC
struct evalTime //struct to help evaluate the this program
{
  struct timeval utime;
  struct timeval stime;
};
bool usagebool = false;
void usageData(int who, struct evalTime *total)
{
  struct rusage timer;
  if((getrusage(who, &timer)))
    {
      exitStat = 1;
      fprintf(stderr, "error, unable to run getrusage.\n");
	fflush(stderr);
	usagebool = false;
    }
    else
      {
	usagebool = true;
	total->utime = timer.ru_utime;
	total->stime = timer.ru_stime;
      }
}
//struct of command options to look for
 static struct option long_options[] = {

					{"rdonly",    required_argument, 0, 'r'},
					{"wronly",   required_argument, 0, 'w'},
					{"verbose", no_argument, 0, 'v'},
					{"command",    required_argument, 0, 'c'},
					{"append", no_argument, 0, APPEND},
					{"cloexec", no_argument, 0, CLOEXEC},
					{"creat", no_argument, 0, CREAT},
					{"directory", no_argument, 0, DIRECTORY},
					{"dsync", no_argument, 0, DSYNC},
					{"excl", no_argument, 0, EXCL},
					{"nofollow", no_argument, 0, NOFOLLOW},
					{"nonblock", no_argument, 0, NONBLOCK},
					{"rsync", no_argument, 0, 'n'}, 
					{"sync", no_argument, 0, SYNC},
					{"trunc",no_argument, 0, TRUNC},
					{"rdwr", required_argument, 0, 'd'},
					{"pipe", no_argument, 0, 'p'},
					{"wait", no_argument, 0, 't'},
					{"close", required_argument, 0, 's'},
					{"abort", no_argument, 0, 'a'},
					{"catch", required_argument, 0, 'h'},
					{"ignore", required_argument, 0, 'i'},
					{"default", required_argument, 0, 'f'},
					{"pause", no_argument, 0, 'u'},
					{"profile", no_argument, 0, 'l'},
					{0, 0, 0, 0}
 };
 int *fileArrays;
 bool openFile(char* fileNam, int io) //this function takes a file name and a flag io returns true if file opening was successful
 {
   numOfFilesOpened++;  
   if(io == 1)//read only case
     {
       int fdi =  open(fileNam, O_RDONLY|trivalOptions, 0666);
       dup2(fdi, (numOfFilesOpened+2));
       fileArrays = realloc(fileArrays, sizeof(int) *numOfFilesOpened);
       fileArrays[numOfFilesOpened-1] = fdi;
       trivalOptions = 0;
	 if(fdi < 0)
	   {
	return false;

	   }
	
	  
     }
   else if(io == 0)//write only case
     {
       int fdo = open(fileNam, O_WRONLY|trivalOptions, 0666);
       dup2(fdo, (numOfFilesOpened+2));
       fileArrays = realloc(fileArrays, sizeof(int) * numOfFilesOpened);
       fileArrays[numOfFilesOpened-1] = fdo;
       trivalOptions = 0;
       if(fdo < 0)
	 {
	 return false;
	 }

     }
   else if(io == 2) // read/write case
     {
       int fdi = open(fileNam, O_RDWR|trivalOptions, 0666);
       dup2(fdi, (numOfFilesOpened+2));
       fileArrays = realloc(fileArrays, sizeof(int) *numOfFilesOpened);
	fileArrays[numOfFilesOpened-1] = fdi;
       trivalOptions = 0;

       if(fdi < 0)
	 {
	 return false;
	 }
      
     }

   return true;
 }
 //handle signal for catch case
 void sig_handle(int x)
 {
   fprintf(stderr, "%d caught\n", x);
   
   exit(x);

 }

 //Timeval_subtract is from the GNU man page:
//https://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

 int main(int argc, char** argv)
 {
   bool commandBool = false;
   struct evalTime totalTime, start_time, end_time;
   
   
   char* argvC[argc];
   for(int i = 0; i != argc; i++)
     {
       argvC[i] = argv[i];
     }
   int profileBool = 0;
   struct commandList{
     int input;
     int output;
     int sterror;
     int size;
     char ** optionList;
   };
    int optionFlag =0;
   int *validPID;
   int *commandStartEnd;
   pid_t *pIDs;
   int indexedComms = 0;
   int counterCommands = 0;
   int largestExit = 0;
   
   struct  commandList commands;

   int indexArg;
   int counter = 0;
   int x = 0; //use this for indexing through optionList
   int verboseFlag= 0;
   int f0, f1, f2; //user specified files 
   bool waitBool = false;
   usageData(RUSAGE_SELF, &totalTime);
   while(1)
     {
       int options = 0;
       int optionIndex = 0;
          if(optionFlag)
	 {

	   optind = indexArg;
	   optionFlag = 0;
	 }
   
       options = getopt_long(argc, argv, "", long_options, &optionIndex);
   

       if(options == -1) //no more options                                     
	 break;


       //start profiling
       usageData(RUSAGE_SELF, &start_time);
   

       switch(options)
	 {
	 case 'l':
	   if(verboseFlag)
	     {
	       printf("--profile\n");
	       fflush(stdout);
	     }
	   // fprintf(stderr, "profilebool = %d\n", profileBool);
	   profileBool = 2;
	   break;
	 case APPEND:
	 case CLOEXEC:
	 case CREAT:
	 case DIRECTORY:
	 case DSYNC:
	 case EXCL:
	 case NOFOLLOW:
	 case NONBLOCK:
	 case SYNC:
	 case TRUNC:
	 case 'n':
	   if(options == 'n')
	     trivalOptions |= O_RSYNC;
	   else
	   trivalOptions |= options;
	   if(verboseFlag)
	     {
	       printf("%s ", optarg);
	       fflush(stdout);
	     }

	   break;

	 case 'p':
	   if(verboseFlag)
	     {
	       printf("--pipe\n");
	       fflush(stdout);
	     }
	   numOfFilesOpened += 2;
	   int index  = numOfFilesOpened -1;
	   if(pipe(pipeArray) == -1)
	     {
	       exitStat = 1;
	       fileArrays[numOfFilesOpened-1] = -1;
	       fileArrays[numOfFilesOpened-2] = -1;
	       fprintf(stderr, "Unable to create pipe.\n");
	       fflush(stderr);
	     }
	   else
	     {


	   int* resizeFileArray = realloc(fileArrays, sizeof(int) * numOfFilesOpened);
	   fileArrays = resizeFileArray;

	   for(int i = 0; i<2; i++)
	     {
	       fileArrays[index++] = pipeArray[i];

	     }
	     }
	   break;
	 case 'a':
	   if(verboseFlag)
	     {
	       printf("--abort\n");
	       fflush(stdout);
	     }
	   *segError = 1;
	   break;
	 case 'i':
	   if(verboseFlag)
	     {
	       printf("--ignore\n");
	       fflush(stdout);
	     }
	   signal(atoi(optarg), SIG_IGN);

	   break;
	 case 'f':
	   if(verboseFlag)
	     {
	       printf("--default\n");
	       fflush(stdout);
	     }
	   signal(atoi(optarg), SIG_DFL);
	   break;
	 case 'u':
	   if(verboseFlag)
	     {
	       printf("--pause\n");
	       fflush(stdout);
	     }
	   pause();
	   break;
	 case 's':
	   if(verboseFlag)
	     {
	       printf("--close %d ", atoi(optarg));
	       printf("\n");
	       fflush(stdout);
	     }
	   // fprintf(stderr, "fileArray is: %d", fileArrays[atoi(optarg)]);
	    close(atoi(optarg)+3);
	     fileArrays[atoi(optarg)] = -1;
	   break;
	 case 'h':
	   if(verboseFlag)
	     {
	       printf("--catch");
	       fflush(stdout);
	     }
	   signal(atoi(optarg), sig_handle);
	   break;
	 case '?':
	   exitStat = 1;
	   fprintf(stderr, "Invalid option: ");
	   fflush(stderr);
	   break;
	 case 'd':
	   if(verboseFlag)
	     {
	       printf("--rdwr %s \n", optarg);
	       fflush(stdout);
	     }
	  
	   if(!openFile(optarg, 2))
	     {
	       exitStat = 1;
	       fprintf(stderr, "error: couldn't open file\n");
	       fflush(stderr);
	     }
	   break;
	 case 'r':
	   if(verboseFlag)
	     {
	       printf("--rdonly %s \n", optarg);
	       fflush(stdout);
	     }
	  
	   if(!openFile(optarg, 1))
	     {
	       exitStat = 1;
	       fprintf(stderr, "error: couldn't open file\n");
	       fflush(stderr);
	     }
	   
	   break;
	 case 'w':
	   if(verboseFlag)
	     {
	       printf("--wronly %s \n", optarg);
	       fflush(stdout);
	     }
	  
	   if(!openFile(optarg, 0))
	     {
	       exitStat = 1;
	       fprintf(stderr,"Error: couldn't create or open\n ");
	       fflush(stderr);
	     }
	   break;
	 case 'v':
	   if(verboseFlag)
	     {
	       printf("--verbose");
	       fflush(stdout);
	     }
	   verboseFlag = 1;
	   break;
	 case 'c':
	   indexArg = optind-1; //start from just after --command

	   // profiler = "command";
	   if(numOfFilesOpened-1 < atoi(argv[indexArg]) || numOfFilesOpened-1 < atoi(argv[indexArg+1]) || numOfFilesOpened-1 < atoi(argv[indexArg+2]))
	     {

	       exitStat = 1;
	       fprintf(stderr, "Error must have at least 3 files specified for --command:\n");
	       fflush(stderr);
	       break;

	     }
	   if(fileArrays[atoi(argv[indexArg])] == -1 || fileArrays[atoi(argv[indexArg+1])] == -1 || fileArrays[atoi(argv[indexArg+2])] == -1)
	     {
	       exitStat = 1;
	       fprintf(stderr, "Error bad file descriptor");
	       fflush(stderr);
	       break;
	     }
	   indexArg = optind+2; //start from just after --command #i #j #k
	   // printf("%d\n\n\n", optind);
	   while(1)
	     {//check through argv for command options
	       if(indexArg == argc) //end of argv
		 break;
	       if(argv[indexArg][0] == '-')
		 if(argv[indexArg][1] == '-')//new command found
		   break;
	       counter++;//keep track of size for malloc
	       indexArg++;//increment through argv for commands
	     }

	   if(!counter)
	     {
	       exitStat = 1;
	       fprintf(stderr, "error --command needs at least one argument: \n");
	       fflush(stderr);
	     }
	   if(verboseFlag)
	     {
	       printf("--command %s %s %s ", argv[optind-1], argv[optind], argv[optind+1]);
	       fflush(stdout);
	       for(int i = optind+2; i != counter+(optind+2); i++)
		 printf("%s ", argv[i]);
	       printf("\n");

	       fflush(stdout);
	     }

	   fflush(stdout);
	   commands.optionList = (char**)malloc((counter+1)*sizeof(char*)); //locate space for number of strings

	   indexArg = optind+2; //reset counter ignore the first 3 file descriptors
	   while(1)
	     {

	       if(indexArg == argc)
		 {
		  
		   break;
		 }

	       if(argv[indexArg][0] == '-')
		 {
		   if(argv[indexArg][1] == '-')
		     {
		       x=0;
		      
		       //printf("%d", optind);
		       break;
		     }

		  
		   optionFlag= 1;
		 }
	       //fill list of commands
	       commands.optionList[x] = argv[indexArg]; //add commands
	       indexArg++;
	       x++;
	     }
	   commands.size = counter+1;
	   commands.optionList[counter] = NULL;
	 
	  
	   counter = 0;
	  
	   //create fork here 
	   pid_t child_ID = fork();
	 
	   if(child_ID < 0)
	     {
	       fprintf(stderr, "failed to fork\n");
	       exitStat = 1;
	       fflush(stderr);
	     }
	   else if(child_ID == 0)
	     {
	      
	       //set file descpt and execute ifd ofd
	       f0 = atoi(argv[optind-1]);
	       f1 = atoi(argv[optind]);
	       f2 = atoi(argv[optind+1]);
	      
	       bool errorCheck = false;
	       if(dup2(f0+3,0) < 0)
		 errorCheck = true;

	       if(dup2(f1+3,1) < 0)
		 errorCheck = true;

	       if(dup2(f2+3,2) < 0)
		 errorCheck = true;

	       if(errorCheck)
		 {
		   exitStat = 1;
		   fprintf(stderr, "Error redirecting files\n");
		   fflush(stderr);
		 }


	       for(int i = 3; i <= numOfFilesOpened+2; i++)
		 {
		   if(close(i) == -1)
		     {
		       exitStat = 1;
		       fprintf(stderr, "Couldn't close files properly\n");
		       fflush(stderr);
		     }
		 }
	      
	       char* cmd = commands.optionList[0];
	      
	       if(execvp(cmd, commands.optionList) == -1)
		 {
		   fprintf(stderr, "fail to execute %s\n" , cmd);
		   exitStat = 1;
		   fflush(stderr);
		 }

		exit(exitStat);

	      }
	    else
	      {

		//profile
		if(profileBool)
		commandBool = true;
		//Create arrays used to hold PIDs and commands being worked on
		counterCommands++;
		 if(counterCommands == 1)
		  {
		    commandStartEnd = malloc(sizeof(int)*counterCommands*2);
		    pIDs = malloc(sizeof(pid_t)*counterCommands);
		    validPID = malloc(sizeof(int)*counterCommands);
		  }
		 else
		  {
		    int *resizeCommand = realloc(commandStartEnd, sizeof(int)*counterCommands*2);
		    pid_t *resizePIDS = realloc(pIDs, sizeof(pid_t)*counterCommands);
		    int *resizeValidPID = realloc(validPID, sizeof(int)*counterCommands);
		    commandStartEnd = resizeCommand;
		    pIDs = resizePIDS;
		    validPID = resizeValidPID;
		  }

		 pIDs[counterCommands -1] = child_ID;
		 validPID[counterCommands - 1] = 0;
		 commandStartEnd[(counterCommands-1)*2] = optind+2;
		 commandStartEnd[(counterCommands-1)*2+1] = indexArg;


		 for(int i = commandStartEnd[counterCommands*2]; i < commandStartEnd[counterCommands*2+1]; i++)
		   printf("%s\n", argv[i]);
		       fflush(stdout);
		     
	      }
	  
	    break;
	  case 't':
	    if(verboseFlag)
	      {
		printf("--wait\n");
		fflush(stdout);
	      }
	    waitBool = true;
	     int pidStatus;
  pid_t tPID;
  int increm = 0;
   int z;
  
   //wait for children to finish here
   while(increm < (counterCommands - indexedComms))
  {
    //check all PIDs that need to be waited on
      for(z = 0; z < counterCommands; z++)
      {
	if(validPID[z]) 
	  {
	      continue;
	  }
	tPID = waitpid(pIDs[z],&pidStatus,WNOHANG);
	  if(tPID != 0)
	  {
	      validPID[z] = 1;
	      increm++;
	      if(WIFEXITED(pidStatus))
	      {
		  int returnSig = WEXITSTATUS(pidStatus);

		  printf("exit %d ", returnSig);
		  fflush(stdout);
		  if(returnSig > largestExit)
		      largestExit = returnSig;
	      }
	      else if(WIFSIGNALED(pidStatus))
	      {
		  int returnSig =  WTERMSIG(pidStatus);
		  printf("signal %d ", returnSig);
		  fflush(stdout);
		   if((returnSig + 128) > largestExit)
		      largestExit = (returnSig+128);
	      }


		 for(int i = commandStartEnd[z*2]; i < commandStartEnd[z*2+1]; i++)
		   printf("%s ", argvC[i]);
		 printf("\n");
		       fflush(stdout);	    
	  }
      }

   }
  
  indexedComms += increm;
  
	    break;



	 }//end of switch
       if(profileBool == 1)
	 {
	  
	   usageData(RUSAGE_SELF, &end_time);

	   if(usagebool)
	     {
	   struct evalTime results;
	   timeval_subtract(&results.utime, &end_time.utime, &start_time.utime);
	   timeval_subtract(&results.stime, &end_time.stime, &start_time.stime);

	  
	   printf("usage data: --%s user time %lds %ldμs Kernel time %lds %ldμs\n", long_options[optionIndex].name, results.utime.tv_sec, results.utime.tv_usec, results.stime.tv_sec, results.stime.tv_usec);
	  	  
	   fflush(stdout);
	     }
	 }
       else if(profileBool == 2)
	 profileBool = 1;
     }//end of while
   if(commandBool && (profileBool != 0))
     {
       usageData(RUSAGE_CHILDREN, &end_time);
           // fprintf(stderr, "%d endtime \n", end_time.utime);                                                                                                     
           if(usagebool)
             {
     	   printf("Children usage data: user time %lds %ldμs Kernel time %lds %ldμs\n", end_time.utime.tv_sec, end_time.utime.tv_usec, end_time.stime.tv_sec, end_time.stime.tv_usec);
	   fflush(stdout);
	     }

     }

    if(waitBool && (largestExit > exitStat))
     {
       if(largestExit >= 128)
	 {
	   signal(largestExit-128, SIG_DFL);
	   raise(largestExit-128);
	 }
       exit(largestExit);
     }
   
   exit(exitStat);
 }//end of main


