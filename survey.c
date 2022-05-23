#include<ncurses.h>
#include<stdlib.h>
#include<string.h>
#include<libxml/parser.h>
#include<libxml/tree.h>

#define MAX_QCHARS 50 //max chars in question buffer
#define MAX_ACHARS 50 //max chars in answer buffer
#define MAX_QS     10 //max questions
#define QSPACE     4  //gap between questions
#define BANNER     4  //space of the survey heading in vertical units

#define MAX_FILE_PATH 200

//#define WIN_V 60
//#define WIN_H 120

void printDebug(int*,int*,char*,int*);
void moveCursor(int,int*,int*,int*);
void editField(int*,int*,char[]);
void forceQuit(void);

int main(int argc,char *argv[])
{

    // process cargs
    char survey_file_path[MAX_FILE_PATH] = {'\0'};
    printf("There are %d command line args\n", argc);
    for ( int i = 0; i < argc; i++ ) {
      if (!strcmp(argv[i],"-s")) { //look for survey file flag
        if (i == argc) {
          printf("You need to supply a file to -s"); //check one error case (others not handled yet)
          exit(10);
        } else {
          i++; 
          strcpy(survey_file_path,argv[i]); 
          printf("found survey file %s\n",survey_file_path);
        }
      }
      printf("%d %s\n",i,argv[i]);
    }
    
    printf("%s has %zu length\n",survey_file_path,strlen(survey_file_path));
    if (sizeof(survey_file_path) == 0) {
      printf("no file supplised with -s");
    }
    

    ///////////////////////
    //    Read in data 
    ///////////////////////
    

    //open the target test.xml
    //char *home = getenv("HOME");
    /*
    printf("opening %s for reading\n",survey_file_path);
    FILE *sfprt; //should be checking if this is a valid pointer
    if (sfprt = fopen(survey_file_path,"r")) { //open and null check
      fclose(sfprt); 
    } else {
      printf("FILE %s not found",survey_file_path);
      exit(6);
    }
    */
 
    xmlDocPtr doc;
    //file exists?
    doc = xmlReadFile(survey_file_path,NULL,0);
    if (doc == NULL) {
      printf("failed to parse file %s\n",survey_file_path);
      xmlFreeDoc(doc);
      exit(5);
    } 
    //validation
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
      printf("empty xml document\n");
      exit(6);
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "survey")) {
      printf("document root of wrong type, root node!=survey , has %s", cur->name);
      exit(7);
    }


    char questions[MAX_QS][MAX_QCHARS+1] = {{'\0'}};
    char answers[MAX_QS][MAX_ACHARS+1] = {{'\0'}};

    int nq = 0;
    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
      //iterate through the node list of the current node
      //until none are left (if they are of type questio process further)
      if ( (!xmlStrcmp(cur->name,(const xmlChar *)"question"))) {
        //iterate through the node list of questions
        //if they contains child nodes called "prompt"
        //get the value they contain
        xmlNodePtr bur = cur->xmlChildrenNode;
        while (bur != NULL) {
          if ((!xmlStrcmp(bur->name, (const xmlChar*)"prompt"))) {
            //printf("prompt %s\n",xmlNodeListGetString(doc,bur->xmlChildrenNode,1));
            //rquestions[num_q] = xmlNodeListGetString(doc,bur->xmlChildrenNode,1); 
            strcpy(questions[nq],xmlNodeListGetString(doc,bur->xmlChildrenNode,1)); 
            nq++; 
          }
          bur = bur->next;
        }
      }
      cur = cur->next;
    }

    //ad hoc test to see if questions are loading in correctly
    int i = 0;
    int end = 0;
    while (!end && i < MAX_QS) {
      if (strlen(questions[i]) == 0) {
        end = 1;
      } else {
        printf("%s\n",questions[i]);
        i++;
      }
    }

    //exit(0);
     

    
    //xmlFreeDoc(doc)  free xml file handler




    // dummy questions set
    int  eq = nq+1; //total item entries (questions and enter)
    /*
    char questions[4][MAX_QCHARS+1] = {
      {" Did you meditate this morning?  "},
      {" Did you bring lunch today?      "},
      {" How did you sleep?              "},
      {" How are you feeling?              "}
    };
    char answers[4][MAX_ACHARS+1] = { "" };
    */

    initscr();  // this starts ncurses
    int maxl,maxc;
    getmaxyx(stdscr,maxl,maxc);
    wresize(stdscr,maxl,maxc); //guess of supported screen size (based on vim lns)
    noecho();  // disable echo
    raw();     // disable line buffering
    keypad(stdscr,TRUE); //Get access to F1,F2.... for our window (stdscr)

    //check if there are more questions than that would fit in the window
    

    
    ////////////////////////
    //   Compatibility Checks
    ////////////////////////

    //check cursor state supports
    if ( curs_set(0) == ERR || curs_set(1) == ERR || curs_set(2) == ERR ) {
      printf("required cursor states are not supported on your terminal");
      exit(3);
    } else {
      curs_set(0);  //hide the cursor
    }

    //color support
    if (!has_colors()) {
      addstr("your terminal does not support colors, aborting");
      refresh();
    }
    if (start_color() != OK) {
      addstr("Unable to start colors, aborting");
      refresh();
      exit(1);
    }


    //setup color combinations
    init_pair(1,COLOR_MAGENTA,COLOR_BLACK); //questions
    init_pair(2,COLOR_BLUE,COLOR_BLUE);     //answer field (blocked)
    init_pair(3,COLOR_WHITE,COLOR_GREEN);   //submit text

    /////////////////////////
    //    Print Layout
    /////////////////////////

    addstr("Daily Survey");
    for (int i = 0; i < BANNER; i++) {
      addstr("\n");
    } 

    //Print out all questions
    attron(COLOR_PAIR(1));

    //source x and y  | location of where the first question is printed    
    int sy,sx;
    getyx(stdscr,sy,sx);  

    //Print question and response fields
    for ( int i = 0; i < nq; i++) {
      printw("    [%d]  %s",i,questions[i]);
      //print out answer field block
      int y,x;
      getyx(stdscr,y,x);
      move(y,MAX_QCHARS);
      attron(COLOR_PAIR(2));
      for ( int k = 0; k < MAX_QCHARS; k++) {
        addch(' ');
      }
      attron(COLOR_PAIR(1));
      for ( int j = 0; j < QSPACE; j++) {
        printw("\n");
      }
    }

    //Print Submit Button
    for ( int i = 0; i < QSPACE; i++) {
    printw("\n");
    }
    attron(COLOR_PAIR(3));
    printw(" Submit \n");
    attron(COLOR_PAIR(1));
    refresh();

    //debug buffer location
    int dby,dbx;
    getyx(stdscr,dby,dbx); 
    dbx = 0;

    //print out form default locationin indicator  
    int selectIndex = 0;
    moveCursor(0,&sy,&selectIndex,&nq);

    //read in keypresses until 'q' is pressed
    //to be replaced with enter on ENTER segment
    int uch;
    int ok = 1;
    USERREAD:while (ok) { 
      uch = getch();
      switch(uch) {
        case KEY_UP :
          printDebug(&dby,&dbx,"Up Key Pressed",&selectIndex);

          if ( selectIndex != 0 ) {
          printDebug(&dby,&dbx,"Up Key p bdp",&selectIndex);
          moveCursor(-1,&sy,&selectIndex,&nq); 
          }
          break;
        case KEY_DOWN :  
          printDebug(&dby,&dbx,"Down Key Pressed",&selectIndex);

          if ( selectIndex != eq-1 ) {
          moveCursor(1,&sy,&selectIndex,&nq); 
          }
          break;
        //case KEY_STAB :  
        case '\t' : //unsure if ncurses has a built in tab constant
          printDebug(&dby,&dbx,"Tab Key Pressed",&selectIndex);
          int offSet = (selectIndex == eq-1) ? -eq + 1 : 1;
          moveCursor(offSet,&sy,&selectIndex,&nq); 
          break;
        case 10 : //KEY_ENTER is for ENTER on numeric key pad , 10 is normal enter
          if (selectIndex == eq-1) {
            printDebug(&dby,&dbx,"Enter Key Pressed (exit attempt)",&selectIndex);
            ok = 0; 
          } else {
            printDebug(&dby,&dbx,"Enter Key Pressed (edit field)",&selectIndex);
            editField(&sy,&selectIndex,answers[selectIndex]);
          }
          break;
        case KEY_BACKSPACE : ;
          int y,x; //if i don't add the above semi colon i get the error
                   //a label can only be part of a statement and a declaration is not a statement..
          getyx(stdscr,y,x);
          move(y,MAX_QCHARS);
          attron(COLOR_PAIR(2));
          for ( int k = 0; k < MAX_QCHARS; k++) {
            addch(' ');
          }
          attron(COLOR_PAIR(1));
          //reset the associated string buffer
          answers[selectIndex][0] = '\0';
          default :
          break;
      }
      refresh();
    }


    int complete = 1;
    for ( int i = 0; i < nq; i++ ) {
      complete &= ( answers[i][0] != '\0');
    }
    if (!complete) {
      printDebug(&dby,&dbx,"Some fields are incomplete",&selectIndex);
      ok = 1;
      goto USERREAD; //lazy
    }
    
    //end ncurses before the program exits
    clear();
    refresh();
    //print out answer buffers
    endwin();

    //unsure why this loop prints doubled ...
    for ( int i = 0; i < nq; i++ ) {
      printf("%d  %s\n",i,answers[i]);
    }
    fflush(stdout);
   
    //save answers to file 
    char *home = getenv("HOME");
    if (home == NULL) {
      printf("unable to find user home");
      exit(4);
    }

    char *fileName = "/save.xml";
    //why the -1 
    //if anything it should be +1 for the null byte
    int pathLength = strlen(home) + strlen(fileName)-1;
    char path[pathLength];

    strncpy(path,home,strlen(home));
    strncat(path,fileName,strlen(fileName));

    printf("opening %s for writing\n",path);
    FILE *fprt; //should be checking if this is a valid pointer
    fprt = fopen(path,"w");
    for ( int i = 0; i < nq; i++) {
      fprintf(fprt,"%s\n",answers[i]);
    }
    fclose(fprt); 

    return EXIT_SUCCESS;
}

/* 
  Print a message out in the debug area 
*/
void printDebug(int* dby,int* dbx,char* msg,int* si) {

  move(*dby,*dbx);
  clrtoeol(); //clear line
  printw("%s  @select %d",msg,*si);
  refresh();
}

/* delete the previous cursor character, then
   move to the new location and write the new 
   location character
   @x     - relative new position
   @sy    - pointer to source y
   @selectIndex - pointer to current question index
*/
void moveCursor(int dcy,int *sy,int *selectIndex,int *nq) {

  //delete the last cursor 
  move((*sy)+(QSPACE*(*selectIndex)),0);
  addch(' ');
  //update selected index
  *selectIndex = (*selectIndex) + dcy;
  //draw new cursor
  move((*sy)+(QSPACE*(*selectIndex)),0);
  printw(">");


  refresh();
}

//need to ban all control/special characters , but
//have a special process for backspace and clear
//WARNING! not checking for buffer overflow
void editField(int *sy,int *selectIndex,char answer[]) {
  //move cursor to start of field 
  //enable cursor
  int prevCursorState = curs_set(2);
  move(QSPACE+(*sy)+(QSPACE*(*selectIndex)),MAX_QCHARS);

  //get number of chars in the buffer
  int charCount = 0;
  int end = 0;
  int charPos = 0;
  int ok = 1;
  while ( charPos < MAX_QCHARS && !end) {
    if (answer[charPos] == '\0') {
      end = 1;
    } else {
      charPos++;  
    }
  }
  //charPos--;
  while (ok) {
    int ic = getch();
    switch(ic) { 
      case 'q':
        forceQuit();
        break;
      case 10 :
        ok = 0;
        break;
      /* this does not behave as anticipated, i see
         that the block spaces disappear on backspace
         but I explicitly add one back , unsure of what is
         going on here
      */
      case KEY_BACKSPACE :;
        attron(COLOR_PAIR(2));
        addch('X');
        attron(COLOR_PAIR(1));
        int y,x; 
        getyx(stdscr,y,x);
        move(y,x-2);
        delch();
        move(y,x-2);
        answer[charPos] = '\0';
        charPos--;
        break;
      default :
        if (charPos < MAX_ACHARS-1) {
          //do not write control characters
          if (! ( ic <= 31 || ic == 127 ) )
            addch(ic);
            answer[charPos] = ic;
            answer[charPos+1] = '\0';
            charPos++; 
        }
    }
    refresh();
    
  }
  move(QSPACE+(*sy)+(QSPACE*(*selectIndex)),0);
  curs_set(prevCursorState);
}

void forceQuit() {
  clear();
  refresh();
  endwin();
  fflush(stdout);
  exit(0);
}


