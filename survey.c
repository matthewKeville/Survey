#include<ncurses.h>
#include<stdlib.h>
#include<string.h>
#include<libxml/parser.h>
#include<libxml/tree.h>

#define MAX_QCHARS 50 //max chars in question buffer
#define MAX_ACHARS 50 //max chars in answer buffer
#define MAX_QS     30 //max questions
#define QSPACE    8// 6  //gap between questions
#define BORDER    2  //vertical units of header and footer

#define MAX_FILE_PATH 200

struct survey_pad {
  WINDOW *pad;
  int padr_off; //where the pad should start in the window
  int padc_off; // ...
  int pad_view_height; //viewport height
  int pad_view_width;  //viewport width
  int vqs; //number of visible questions
  int nq;
};


void printDebug(int*,int*,char*,int*);
void moveCursor(int,int*,int*,int*);

void editField(struct survey_pad*,int,int,char[]);
void moveIndex(int,int *,int *,struct survey_pad*);
void forceQuit(void);



int main(int argc,char *argv[])
{
    //////////////////
    // process cargs
    //////////////////

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
    //xmlFreeDoc(doc)  free xml file handler

    int  eq = nq+1; //total item entries (questions and enter)

    ///////////////////////////
    // Curses Config
    ///////////////////////////

    initscr();  // this starts ncurses
    int maxl = 36;
    int maxc = 120;
    wresize(stdscr,maxl,maxc); //guess of supported screen size (based on vim lns)
    noecho();  // disable echo
    raw();     // disable line buffering
    keypad(stdscr,TRUE); //Get access to F1,F2.... for our window (stdscr)

    /////////////////////////////
    //   Compatibility Checks
    /////////////////////////////

    //check cursor state supports
    if ( curs_set(0) == ERR || curs_set(1) == ERR || curs_set(2) == ERR ) {
      printf("required cursor states are not supported on your terminal");
      exit(3);
    } else {
      curs_set(0);  //hide the cursor
    }

    //color support
    if (!has_colors()) {
      printw("your terminal does not support colors, aborting");
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
 
    char submit_key_string[] = "F1";
    //int submit_key = KEY_F(1);
    //BANNER
    addstr("Daily Survey\n");
    printw("PRESS %s , to submit",submit_key_string);

    //SUBMIT BUTTON
    attron(COLOR_PAIR(3));
    //mvprintw(maxl-BORDER-1,0," Submit \n");
    attron(COLOR_PAIR(1));
    refresh();

   

    //SURVEY CONTENT
    attron(COLOR_PAIR(1));
    //source x and y  | location of where the first question is printed    
    int sy,sx;
    getyx(stdscr,sy,sx);  

    //should be based on number of questions (row-wise);
    int padspacel = MAX_QS*(QSPACE+1);
    if (padspacel < maxl - (2*BORDER)) {
      padspacel = maxl - ( 2*BORDER);
    }
    WINDOW *pad = newpad(padspacel,maxc);

    for ( int i = 0; i < nq; i++) {
      wattron(pad,COLOR_PAIR(1));
      //question
      mvwprintw(pad,i*QSPACE,0,"    [%d]  %s",i,questions[i]);
      wattron(pad,COLOR_PAIR(2));
      //answer field
      wmove(pad,i*QSPACE,MAX_QCHARS);
      for ( int k = 0; k < MAX_QCHARS; k++) {
        waddch(pad,' ');
      }
    }
   
    //viewing window for pad space is (maxlines - 2*BORDER) , running the full window
    //pminrow , pmincol , sminrow smincol , smaxrow , smaxcol
    
    int padr_off = BORDER;
    int padc_off = 0;
    int pad_view_height = (maxl-(2*BORDER));
    int pad_view_width  = maxc;

/*
struct SurveyPad {
  WINDOW *pad;
  int *padr_off; //where the pad should start in the window
  int *padc_off; // ...
  int *pad_view_height; //viewport height
  int *pad_view_width;  //viewport width
};
*/


    //DEBUG BUFFER
    int dby = BORDER+pad_view_height;
    int dbx = 0;

    prefresh(pad,0,0,padr_off,0,pad_view_height + padr_off,pad_view_width);
    //display pad outer edges
    mvprintw(padr_off-1,0,"-------PADDING EDGE-------");
    mvprintw(1+padr_off+pad_view_height,0,"------PADDING EDGE--------");
    refresh();

    //calculate number of viewable questions
    int vqs = pad_view_height/QSPACE + 1;
    mvprintw(dby,0,"vqs is %d",vqs); //debug output
    int pad_segment = 0; //what segement of the pad we are on 

    //print out form default location indicator  
    int selectIndex = 0;
    int before = wattron(pad,COLOR_PAIR(1));
    mvwaddch(pad,QSPACE*selectIndex,0,'>'); 
    wattron(pad,COLOR_PAIR(before));
    prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,0,pad_view_height + padr_off,pad_view_width);

    struct survey_pad spad = {
      .pad             = pad,
      .padr_off        = padr_off,
      .padc_off        = padc_off,
      .pad_view_height = pad_view_height,
      .pad_view_width  = pad_view_width,
      .vqs             = vqs,
      .nq              = nq
    };


    //read in keypresses until 'q' or F1 are pressed
    int uch;
    int ok = 1;
    USERREAD:while (ok) { 
      uch = getch();
      switch(uch) {
        case 'k'    : //fall through
        case KEY_UP :
          moveIndex(-1,&selectIndex,&pad_segment,&spad);
          break;
        case 'j' : //fall through
        case KEY_DOWN :  
          moveIndex(1,&selectIndex,&pad_segment,&spad);
          break;
        case '\t' : 
          //for now just forward tab, but in the future cycle tab
          //will require reworked moveIndex (unless I want a hacky
          //for loop )
          moveIndex(1,&selectIndex,&pad_segment,&spad);
          break;
        case 10 : //KEY_ENTER is for ENTER on numeric key pad , 10 is normal enter 

          //void editField(struct survey_pad *spad,int pad_segment,int selectIndex,char answer[]) {
          editField(&spad,pad_segment,selectIndex,answers[selectIndex]);
          break;
        case KEY_F(1) : 
          ok = 0;
          break;
        case KEY_BACKSPACE : ;
          /*
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
          */
          break;
      case 'q':
        forceQuit();
        break;
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
      //printDebug(&dby,&dbx,"Some fields are incomplete",&selectIndex);
      mvprintw(dby,0,"Some fields are incomplete");
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

//sy is problematic

/* delete the previous cursor character, then
   move to the new location and write the new 
   location character
   @x     - relative new position
   @sy    - pointer to source y
   @selectIndex - pointer to current question index
*/
/*
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
*/

//need to ban all control/special characters , but
//have a special process for backspace and clear
//WARNING! not checking for buffer overflow
void editField(struct survey_pad *spad,int pad_segment,int selectIndex,char answer[]) {
  //needs all this shit, maybe it's time to add structs

  //get number of chars in the buffer
  //can probably replace with strlen ...
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

  while (ok) {
    int ic = getch();
    switch(ic) { 
      case 10 :
        ok = 0;
        break;
      /* this does not behave as anticipated, i see
         that the block spaces disappear on backspace
         but I explicitly add one back , unsure of what is
         going on here
      */
      case KEY_BACKSPACE :;
        /*
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
        */
      default :
        if (charPos < MAX_ACHARS-1) {
          //do not write control characters
          if (! ( ic <= 31 || ic == 127 ) )
            mvwaddch(spad->pad,QSPACE*(selectIndex),MAX_QCHARS+charPos,ic);
            answer[charPos] = ic;
            answer[charPos+1] = '\0';
            charPos++; 
            prefresh(spad->pad,pad_segment*spad->vqs*QSPACE,
              0,spad->padr_off,0,spad->pad_view_height + spad->padr_off,spad->pad_view_width);
        }
    }
    refresh();
  }
}

void forceQuit(void) {
  clear();
  refresh();
  endwin();
  fflush(stdout);
  exit(0);
}


void moveIndex(int i,int *selectIndex,int *pad_segment,struct survey_pad* spad) {

  if (i<0) {
    if ( *selectIndex != 0 ) {

      int before = wattron(spad->pad,COLOR_PAIR(1));
      //delete old cursor 
      mvwaddch(spad->pad,QSPACE*(*selectIndex),0,' '); 

      //update index
      (*selectIndex)--;
      //add new cursor
      mvwaddch(spad->pad,QSPACE*(*selectIndex),0,'>'); 
      wattron(spad->pad,COLOR_PAIR(before));

      //move pad if stepping over vqs boundary (visible questions)
      if (((*selectIndex)+1) % spad->vqs == 0) {
        //mvprintw(dby,0,"Upper Boundary reached at %d",(*selectIndex));
        (*pad_segment)--;
      }

    }
 } else {
    if ( *selectIndex != spad->nq-1 ) {

      int before = wattron(spad->pad,COLOR_PAIR(1));
      //delete old cursor 
      mvwaddch(spad->pad,QSPACE*(*selectIndex),0,' '); 

      //update index
      (*selectIndex)++;
      //add new cursor
      mvwaddch(spad->pad,QSPACE*(*selectIndex),0,'>'); 
      wattron(spad->pad,COLOR_PAIR(before));

      if ((*selectIndex) % spad->vqs == 0) {
        //prefresh(pad,selectIndex*QSPACE,0,padr_off,0,pad_view_height + padr_off,pad_view_width);
        //mvprintw(dby,0,"Lower Boundary reached at %d",selectIndex);
        (*pad_segment)++;
      }
    }
 }
 prefresh(spad->pad,(*pad_segment)*(spad->vqs*QSPACE),0,spad->padr_off,0,spad->pad_view_height + spad->padr_off,spad->pad_view_width);
}
/*
case KEY_UP :
          if ( selectIndex != 0 ) {
            int before = wattron(pad,COLOR_PAIR(1));
            //delete old cursor 
            mvwaddch(pad,QSPACE*selectIndex,0,' '); 
            //update index
            selectIndex--;
            //add new cursor
            mvwaddch(pad,QSPACE*selectIndex,0,'>'); 
            wattron(pad,COLOR_PAIR(before));

            //move pad if stepping over vqs boundary (visible questions)
            if ((selectIndex+1) % vqs == 0) {
              mvprintw(dby,0,"Upper Boundary reached at %d",selectIndex);
              pad_segment-=1;
            }

          }
          prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,0,pad_view_height + padr_off,pad_view_width);
          break;
        case 'j' : //fall through
        case KEY_DOWN :  
          if ( selectIndex != nq-1 ) {
            
            int before = wattron(pad,COLOR_PAIR(1));
            //delete old cursor 
            mvwaddch(pad,QSPACE*selectIndex,0,' '); 
            //update index
            selectIndex++;
            //add new cursor
            mvwaddch(pad,QSPACE*selectIndex,0,'>'); 
            wattron(pad,COLOR_PAIR(before));

            if (selectIndex % vqs == 0) {
              //prefresh(pad,selectIndex*QSPACE,0,padr_off,0,pad_view_height + padr_off,pad_view_width);
              mvprintw(dby,0,"Lower Boundary reached at %d",selectIndex);
              pad_segment+=1;
            }
          }
          prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,0,pad_view_height + padr_off,pad_view_width);
          break;
*/




