#include "questions.h"

#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<sys/time.h> //gettimeofday

#include<ncurses.h>
#include<libxml/parser.h>
#include<libxml/tree.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>
#include<libxml/xmlstring.h>

#define BORDER     5    //vertical units of header and footer

#define MAX_FILE_PATH 200
#define MAX_XPATH 200

//will change later 
#define ANSWER_BUFFER_WIDTH   30
#define QUESTION_BUFFER_WIDTH 30
#define MAX_FREE_RESPONSE     50 //to be replaced with user defined max in schema
#define OPTION_PAD             2


/*
struct survey_pad {
  WINDOW *pad;
  int padr_off;         //where the pad should start in the window (in rows)
  int padc_off;         //where the pad should start in the window (in cols)
  int pad_view_height;  //viewport height
  int pad_view_width;   //viewport width
  int vqs;              //number of visible questions
  int nq;               //number of questions
};
*/


void editField(struct survey_pad*,int,int,char[]);
void clearField(struct survey_pad*,int,char[]);
void moveIndex(int);
void forceQuit(void);

WINDOW *pad;
int current_question_index = 0;
int total_question_count;

int main(int argc,char *argv[])
{
    /////////////////////
    // Command Line Args
    /////////////////////

    //Error Cases Not Checked Yet
    char survey_file_path[MAX_FILE_PATH] = {'\0'};
    printf("There are %d command line args\n", argc);
    for ( int i = 0; i < argc; i++ ) {
      if (!strcmp(argv[i],"-s")) {    //look for survey file flag
        if (i == argc) {
          printf("You need to supply a file to -s");  
          exit(1);
        } else {
          i++; 
          strcpy(survey_file_path,argv[i]); 
          printf("found survey file %s\n",survey_file_path);
        }
      }
    }

    //check for empty file string
    if (strlen(survey_file_path) == 0) {
      printf("no file supplised with -s");
      exit(2);
    }
    

    ///////////////////////
    //    Read in data 
    ///////////////////////
    
 
    xmlDocPtr doc;
    doc = xmlReadFile(survey_file_path,NULL,0);
    if (doc == NULL) {
      printf("failed to parse file %s\n",survey_file_path);
      xmlFreeDoc(doc);
      exit(3);
    } 
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
      printf("empty xml document\n");
      exit(4);
    }
    if (xmlStrcmp(cur->name,BAD_CAST "survey")) {
      printf("Invalid file root node!=survey , has %s", cur->name);
      exit(5);
    }


    //////////////////////////
    // SCHEMA Validation   //
    ////////////////////////


    /////////////////////////
    // PARSE XML
    /////////////////////////

    xmlXPathContext* xpathCtx;
    xpathCtx = xmlXPathNewContext(doc);


    //get the type of a question given an object id
    xmlChar * getQuestionType(xmlChar* id) {
      xmlChar idXPath[MAX_XPATH];
      xmlStrPrintf(idXPath,MAX_XPATH,"//question[@id = '%s']/questionType/node()",id);
      xmlXPathObjectPtr XPathOP = xmlXPathEvalExpression(idXPath,xpathCtx);
      xmlChar *value  = xmlNodeGetContent((XPathOP->nodesetval->nodeTab[0])); 
      return value;
    }

    //frs free_response pointer array
    //frc free response count pointer
    //qc question count
    //qhs question header pointer array
    void processFreeResponse(xmlChar *id,free_response **frs,int *frc,question_header **qhs,int *qc) {
      
      //extract the prompt
      xmlChar questionPromptPath[MAX_XPATH]; 
      xmlStrPrintf(questionPromptPath,MAX_XPATH,"//question[@id = '%s']/prompt",id);

      xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
      xmlNodeSet *pnodes = pxoptr->nodesetval;
      xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

      char *prompt = malloc( (strlen((char *)promptXml)+1) * sizeof (char));
      strcpy(prompt,(char *)promptXml);

      //package into free_response struct
      free_response * fr_question = malloc(sizeof(free_response));
      fr_question->prompt = malloc((xmlStrlen(promptXml)+1) * sizeof(char)); //REPLACE_MAX_ACHARS
      strcpy(fr_question->prompt,prompt);
      fr_question->response = malloc((MAX_FREE_RESPONSE+1) * sizeof(char)); //REPLACE_MAX_ACHARS
      frs[(*frc)] = fr_question;
  
      //package into question_header
      question_header * q_header = (malloc(sizeof(question_header)));
      q_header->type=FR;
      q_header->index=(*qc);
      q_header->tindex=(*frc);
      strcpy(q_header->question_id,(char *)id);
      qhs[(*qc)] = q_header;   
      (*qc)++;
      (*frc)++;

    }



    //@mcs : multiple_choice pointer array
    //@mcc : free response counter
    //@qc  : question count
    //@qhs : question header pointer array
    void processMultipleChoice(xmlChar *id,multiple_choice **mcs,int *mcc,question_header **qhs,int *qc) {

      //extract the prompt
      xmlChar questionPromptPath[MAX_XPATH]; 
      xmlStrPrintf(questionPromptPath,MAX_XPATH,"//question[@id = '%s']/prompt",id);

      xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
      xmlNodeSet *pnodes = pxoptr->nodesetval;
      xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

      char *prompt = malloc( (strlen((char *)promptXml)+1) * sizeof (char));
      strcpy(prompt,(char *)promptXml);

      //extract options
      xmlChar multipleChoiceOptionsPath[MAX_XPATH]; 
      xmlStrPrintf(multipleChoiceOptionsPath,MAX_XPATH,"//question[@id = '%s']/multipleChoice/option",id);
      xmlXPathObject * axoptr = xmlXPathEvalExpression(multipleChoiceOptionsPath,xpathCtx);
      xmlNodeSet *anodes = axoptr->nodesetval; 
      int options_size = (anodes) ? anodes->nodeNr : 0;
      
      //package package package package options
      char **optionSet = malloc(options_size * sizeof(char *));
      for ( int j = 0; j < options_size; j++) {
        xmlNode *dur = anodes->nodeTab[j];
        char *value  = (char *)xmlNodeGetContent(dur);
        int optionLength = (int) strlen(value);
        optionSet[j] = malloc((optionLength+1) * sizeof(char)); //REPLACE with macro limit
        strcpy(optionSet[j],value);
      }

      //package into multiple_choice struct
      multiple_choice *mc_question = malloc(sizeof(multiple_choice));
      mc_question = malloc(sizeof(multiple_choice));
      mc_question->prompt = malloc((xmlStrlen(promptXml)+1) * sizeof(char)); //REPLACE_MAX_ACHARS
      strcpy(mc_question->prompt,prompt);
      mc_question->options=optionSet; 
      mc_question->num_options=options_size;
      mc_question->selected=-1;
      mcs[(*mcc)]=mc_question;
  
      //package into question_header
      question_header * q_header = (malloc(sizeof(question_header)));
      q_header->type=MC;
      q_header->index=(*qc);
      q_header->tindex=(*mcc);
      strcpy(q_header->question_id,(char *)id);

      qhs[(*qc)]=q_header;
      (*qc)++;
      (*mcc)++;

  }
        
    //sas : select_all pointer array
    //sac : select_all counter
    //qc  : question count
    //qhs : question header pointer array
    void processSelectAll(xmlChar *id,select_all **sas,int *sac,question_header **qhs,int *qc) {

      //extract the prompt
      xmlChar questionPromptPath[MAX_XPATH]; 
      xmlStrPrintf(questionPromptPath,MAX_XPATH,"//question[@id = '%s']/prompt",id);

      xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
      xmlNodeSet *pnodes = pxoptr->nodesetval;
      xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

      char *prompt = malloc( (strlen((char *)promptXml)+1) * sizeof (char));
      strcpy(prompt,(char *)promptXml);

      //extract question info 
      xmlChar selectAllOptionsPath[MAX_XPATH]; 
      xmlStrPrintf(selectAllOptionsPath,MAX_XPATH,"//question[@id = '%s']/multipleChoice/option",id);

      xmlXPathObject * axoptr = xmlXPathEvalExpression(selectAllOptionsPath,xpathCtx);
      xmlNodeSet *anodes = axoptr->nodesetval; 
      int options_size = (anodes) ? anodes->nodeNr : 0;
      
      //options
      char **optionSet = malloc(options_size * sizeof(char *));
      for ( int j = 0; j < options_size; j++) {
        xmlNode *dur = anodes->nodeTab[j];
        char *value  = (char *)xmlNodeGetContent(dur); 
        int optionLength = (int) strlen(value);
        optionSet[j] = malloc( (optionLength+1) * sizeof(char));
        strcpy(optionSet[j],value);
      }

      //package into select all struct
      select_all * sa_question = malloc(sizeof(select_all));
      sa_question->prompt = malloc((xmlStrlen(promptXml)+1) * sizeof(char)); //REPLACE_MAX_ACHARS
      strcpy(sa_question->prompt,prompt);
      sa_question->options=optionSet; 
      sa_question->num_options=options_size;
      sa_question->selected=calloc(options_size,sizeof(int));
      sas[(*sac)]=sa_question;
  
      //package into question_header
      question_header * q_header = malloc(sizeof(question_header)); 
      q_header->type=SA;
      q_header->index=(*qc);
      q_header->tindex=(*sac);

      qhs[(*qc)]=q_header;
      (*qc)++;
      (*sac)++;
  }

    /////////////////////////////////////////
    //query for amount of each question type
    ///////////////////////////////////////// 
  
    int getCount(const xmlChar* query) {
      xmlXPathObjectPtr XPathOP = malloc(sizeof(xmlXPathObject));
      XPathOP = xmlXPathEvalExpression(query,xpathCtx);
      int x = (XPathOP->nodesetval) ? XPathOP->nodesetval->nodeNr : 0;
      printf("found %d questions\n",x);
      free(XPathOP);
      return x;
    }

    int multiple_choice_count = getCount(BAD_CAST "//question/questionType[text()='MC']");
    int select_all_count      = getCount(BAD_CAST "//question/questionType[text()='SA']");
    int free_response_count   = getCount(BAD_CAST "//question/questionType[text()='FR']");
    int scale_choice_count    = 0;   //getCount(BAD_CAST "//question/questionType[text()='SC']");

    total_question_count           = multiple_choice_count + select_all_count +
                                free_response_count + scale_choice_count;



    /////////////////////////////////////////
    //parse the xml
    /////////////////////////////////////////

    /*
      variables names here are not ideal
        blank_blank_index refers to the current index for each respective question type
        but is also used as (blank_blank_index+1) to get the count of them.
        meanwhile question_count is an actual count
    */

    question_header **question_headers = malloc(total_question_count*sizeof(question_header*));
    free_response **free_responses = malloc(free_response_count*sizeof(free_response*));
    select_all **select_alls = malloc(select_all_count*sizeof(select_all*));
    multiple_choice **multiple_choices = malloc(multiple_choice_count*sizeof(multiple_choice*));
    scale_choice **scale_choices = malloc(scale_choice_count*sizeof(scale_choice*));

    int free_response_index = 0;
    int select_all_index    = 0;
    int multiple_choice_index = 0;
    int scale_choice_index    = 0;
    int question_index        = 0;


    
    //Iterate Through all question id's
    //And parse them into there respective structs
    const xmlChar* allQuestionIdXPath = BAD_CAST "//question/@id";
    xmlXPathObjectPtr XPathOP = xmlXPathEvalExpression(allQuestionIdXPath,xpathCtx);
    xmlNodeSetPtr id_nodes = XPathOP->nodesetval; 
    int question_count = (id_nodes) ? id_nodes->nodeNr : 0;
    printf("found %d questions\n",question_count);

    for (int i = 0; i < question_count; i++) {
      xmlNode *current_id_node = id_nodes->nodeTab[i]; 
      xmlChar *current_id = xmlNodeGetContent(current_id_node); 
      char *question_type = (char*) getQuestionType(current_id);
      printf("Question ID : %s , Type : %s\n", (char *) current_id,question_type);
      
      if (strcmp(question_type,"MC")==0) {
        printf("found multiple choice\n");
        processMultipleChoice(current_id,multiple_choices,&multiple_choice_index,
          question_headers,&question_index);
      } else if (strcmp(question_type,"SA")==0) {
        printf("found select all\n");
        processSelectAll(current_id,select_alls,&select_all_index,
          question_headers,&question_index); 
      } else if (strcmp(question_type,"FR")==0) {
        printf("found free response\n");
        processFreeResponse(current_id,free_responses,&free_response_index,
          question_headers,&question_index); 
      } else if (strcmp(question_type,"SC")==0) {
        //todo
      } else {
        printf("Unkown question type %s, not known to schema\n",question_type);
        exit(21);
      }
      
    }

    printf("testing parsed structures\n");
  
    void print_multiple_choice(int mc_index) {
        multiple_choice *mc = multiple_choices[mc_index];
        printf("\t%s\n",mc->prompt);
        for (int j = 0; j < mc->num_options; j++) {
          printf("\t\t%s\n",mc->options[j]);
          if ( j == mc->num_options-1) { printf("\n"); }
        }
    }

    void print_select_all(int sa_index) {
        select_all *sa = select_alls[sa_index];
        printf("\t%s\n",sa->prompt);
        for (int j = 0; j < sa->num_options; j++) {
          printf("\t\t%s\n",sa->options[j]);
          if ( j == sa->num_options-1) { printf("\n"); }
        }
    }

    void print_free_response(int fr_index) {
      free_response *fr = free_responses[fr_index];
      printf("\t%s\n",fr->prompt);
    }

    /*
    for ( int i = 0; i < select_all_index; i++) {
      select_all *sa = select_alls[i];
      printf("\n%s\n",sa->prompt);
      for (int j = 0; j < sa->num_options; j++) {
        printf("%s\t",sa->options[j]);
      } 
      printf("\n Selected : ");
      for ( int j = 0; j < sa->num_options; j++) {
        printf("%d",sa->selected[i]);
      }
    }
    */

    printf("mc  : %d questions \n",multiple_choice_index); 
    printf("fr  : %d questions \n",free_response_index); 
    printf("sa  : %d questions \n",select_all_index); 
    printf("\n question header test : %d question headers \n",question_index);
    for ( int i = 0; i < question_index; i++) {
      question_header *qh = question_headers[i];
      printf("\nId : %s , Type : %d\n",qh->question_id,qh->type);
      printf("\nQuestion Index : %d , Type Index : %d\n",qh->index,qh->tindex);
        switch(qh->type) {
          case  MC : 
            print_multiple_choice(qh->tindex); 
            break;
          case  FR : 
            print_free_response(qh->tindex); 
            break;
          case  SA : 
            print_select_all(qh->tindex); 
            break;
          case  SC : 
            break;
        }
    }

    //free libxml2's allocated data
    xmlFreeDoc(doc);
   
     
    //exit(0); //exit point for xml-parsing testing
    //here down needs to reworked with new data structures



    /////////////////
    // META DATA
    //////////////////
    time_t t;
    struct tm *start_time;
    time(&t);
    start_time = localtime(&t);
    printf("Started at : %d-%02d-%02d %02d:%02d:%02d\n", start_time->tm_year + 1900,
      start_time->tm_mon + 1, start_time->tm_mday, start_time->tm_hour, start_time->tm_min, start_time->tm_sec);

    struct timeval t1;
    gettimeofday(&t1,NULL);
    printf("time is %ld \n",t1.tv_sec);

    ///////////////////////////
    // Curses Config
    ///////////////////////////

    initscr();  
    //total dimensions on curses main window
    int src_lines = 36;
    int src_cols  = 120;

    wresize(stdscr,src_lines,src_cols);  
    noecho();                   // disable echo
    raw();                      // disable line buffering
    keypad(stdscr,TRUE);        //Get access to F1,F2.... for our window (stdscr)


    /////////////////////////////
    //   Compatibility Checks
    /////////////////////////////

    //check cursor state supports
    if ( curs_set(0) == ERR || curs_set(1) == ERR || curs_set(2) == ERR ) {
      printf("required cursor states are not supported on your terminal");
      exit(6);
    } else {
      curs_set(0);  //hide the cursor
    }

    //color support
    if (!has_colors()) {
      printf("your terminal does not support colors, aborting");
      exit(7);
    }

    if (start_color() != OK) {
      printf("Unable to start colors, aborting");
      exit(8);
    }

    //setup color combinations
    init_pair(1,COLOR_MAGENTA,COLOR_BLACK); //questions
    init_pair(2,COLOR_BLUE,COLOR_BLUE);     //answer field (blocked)
    init_pair(3,COLOR_WHITE,COLOR_GREEN);   //submit text
    attron(COLOR_PAIR(1));

    /////////////////////////
    //    Window Layout
    /////////////////////////

    //demarcate pad/window boundary
    mvaddch(src_lines-2,src_cols-2,'$');
    mvaddch(src_lines-2,0,'$');
 
    char submit_key_string[] = "F1";  //Survey submission key
 
    //Write Survey Banner
    addstr("Daily Survey\n");
    //Submission info
    printw("PRESS %s , to submit | PRESS ENTER , to type,  PRESS BACKSPACE , to clear field",submit_key_string);
    refresh(); 


    /////////////////////////
    //  Generate Survey Pad
    /////////////////////////


    //I'm unsure how to calculate how many lines I will need in the pad, as the new model
    //does not have a strict rule of how much space questions & answers will take up
    //This will require some calculation or upper bound , hence guess ...
    int guess = 500;
    pad = newpad(guess,src_cols-1); 

    //y index of that start of questions
    int *question_y = malloc(total_question_count * sizeof(int));

    //draw survey in pad
    int prev_question_end = 0;
    for ( int i = 0; i < total_question_count; i++) {
      question_header* qh = question_headers[i];
      switch (qh->type) {
        case FR :;
          question_y[i]=prev_question_end; 
          free_response* fr = free_responses[qh->tindex];
          prev_question_end = draw_free_response(fr,prev_question_end);
          break;
        case MC :;
          question_y[i]=prev_question_end; 
          multiple_choice* mc = multiple_choices[qh->tindex];
          prev_question_end = draw_multiple_choice(mc,prev_question_end);
          break;
        case SA :;
          question_y[i]=prev_question_end; 
          select_all* sa = select_alls[qh->tindex];
          prev_question_end = draw_select_all(sa,prev_question_end);
          break;
        case SC :;
          question_y[i]=prev_question_end; 
          //scale_choice* sc = scale_choices[qh->tindex];
          //prev_question_end += 3;
          break;
        default :
          //prev_question_end += 3;
          question_y[i]=prev_question_end; 
          break;
      }
    }
   
    int pad_row_offset = BORDER; //what row does the pad start
    int pad_col_offset = 0;
    int pad_view_height = src_lines-1 - (2*BORDER); //how many rows does it go for/
    int pad_view_width  = src_cols-1;



    //DEBUG PAD BORDER
    mvprintw(pad_row_offset-1,0,"-------PAD BORDER-------"); //just before padd starts
    mvprintw(pad_row_offset+pad_view_height,0,"------PAD BORDER--------");//just after pad ends


    //draw initial question selector
    int selectIndex = 0;
    //mvwaddch(pad,QSPACE*selectIndex,0,'>'); 
    //prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,padc_off,pad_view_height-2+padr_off,pad_view_width);
    prefresh(pad,(question_y[current_question_index]),pad_col_offset,pad_row_offset,pad_col_offset,pad_view_height-2+pad_row_offset,pad_view_width);
                 //// ^ location of current question



    //read in keypresses until 'q' or F1 are pressed
    int uch;
    int ok = 1;
    USERREAD:while (ok) { 
      uch = getch();
      switch(uch) {
        case 'k'    : //fall through
        case KEY_UP :
          moveIndex(-1);
          break;
        case 'j' : //fall through
        case KEY_DOWN :  
          moveIndex(1);
          break;
        case '\t' : 
          moveIndex(1);
          break;
        case 10 : //KEY_ENTER is for ENTER on numeric key pad , 10 is normal enter 
          //void editField(struct survey_pad *spad,int pad_segment,int selectIndex,char answer[]) {
          //editField(&spad,pad_segment,selectIndex,answers[selectIndex]);
          break;
        case KEY_F(1) : 
          ok = 0;
          break;
        case KEY_BACKSPACE : ;
          //clearField(&spad,selectIndex,answers[selectIndex]);

          //prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,padc_off,pad_view_height-2+padr_off,pad_view_width);

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

      prefresh(pad,(question_y[current_question_index]),pad_col_offset,pad_row_offset,pad_col_offset,pad_view_height-2+pad_row_offset,pad_view_width);
      refresh();
    }

    //validate that all questions have been answered
    /*
    int complete = 1;
    for ( int i = 0; i < total_question_count; i++ ) {
      complete &= ( answers[i][0] != '\0');
    }

    if (!complete) {
      ok = 1;
      goto USERREAD; 
    }
    */

    //get elapsed time
    struct timeval t2;
    gettimeofday(&t2,NULL);
    printf("elapsed time is %ld \n",(t2.tv_sec)-(t1.tv_sec));

   
    //clean up ncurses 
    clear();
    refresh();
    endwin();

    fflush(stdout);
    
    //save answers to file 
    /*
    char *home = getenv("HOME");
    if (home == NULL) {
      printf("unable to find user home");
      exit(9);
    }
    */

    /*
    char *fileName = "/save.xml";
    int pathLength = strlen(home) + strlen(fileName)-1;
    char path[pathLength];

    strncpy(path,home,strlen(home));
    strncat(path,fileName,strlen(fileName));

    printf("opening %s for writing\n",path);
    FILE *fprt; //should be checking if this is a valid pointer
    if ((fprt = fopen(path,"w"))) {
      for ( int i = 0; i < total_question_count; i++) {
        fprintf(fprt,"%s\n",answers[i]);
      } 
    } else {
      printf("Unable to save survey");
      exit(10);
    }
    fclose(fprt); 
    */


    return EXIT_SUCCESS;
}


//draw a free response question and return how many lines it took up
//@fr      : a pointer to the question 
//@start_y : the starting row to draw the question 
//@return  :the row immediatly after the end of this question
int draw_free_response(free_response *fr,int start_y) {
 
  wattron(pad,COLOR_PAIR(1));
  int i = 0;
  int c = 0;
  int r = 0;
  //question
  while ( i < strlen(fr->prompt)) {
    if ((c % QUESTION_BUFFER_WIDTH) != QUESTION_BUFFER_WIDTH-1 ) {
      mvwaddch(pad,start_y+r,c,(fr->prompt)[i]);
      c++;
    } else {
      c = 0; 
      r++;
    }
    i++;
  }

  r++;  
  wattron(pad,COLOR_PAIR(2));
  c = 0;

  //needs to be reworked for MAX_FREE_RESPONSE greater than the characters of one line
  while ( c < MAX_FREE_RESPONSE ) {
    mvwaddch(pad,start_y+r,c,'_');
    c++;
  }

  return (r+start_y+1);

}




//draw a multiple question and return how many lines it took up
//@mc      : a pointer to the question 
//@start_y : the starting row to draw the question 
//@return  :the row immediatly after the end of this question
int draw_multiple_choice(multiple_choice *mc,int start_y) {
 
  wattron(pad,COLOR_PAIR(1));
  int i = 0;
  int c = 0;
  int r = 0;
  //question
  while ( i < strlen(mc->prompt)) {
    if ((c % QUESTION_BUFFER_WIDTH) != QUESTION_BUFFER_WIDTH-1 ) {
      mvwaddch(pad,start_y+r,c,(mc->prompt)[i]);
      c++;
    } else {
      c = 0; 
      r++;
    }
    i++;
  }

  r++;  
  c = 0; //place in answer buffer space
  int d = 0; //place in option string
  int option = 0; 
  while (option < mc->num_options) {  //iterate through all options
    while ( d < strlen((mc->options)[option]) -1) { //keep writing characters for the current options till complete
      if ((c % ANSWER_BUFFER_WIDTH) != ANSWER_BUFFER_WIDTH-1 ) {
        mvwaddch(pad,start_y+r,c,((mc->options)[option])[d]);
        //mvwaddch(pad,start_y+r,c,"x");
        c++;
        d++;
      } else {
        c = 0;
        r++;
      }
    }
    //option is complete
    d = 0; 
    c+=OPTION_PAD;//add 4 spaces between options | should be a macro (TBD)
    option++;
    //if next option won't fit on the line add a new line (TBD)
    if ((mc->options)[option]) {
      //handle later (an established max option will be necessary to properly
      //handle this case
    }
  }

  return (r+start_y+1);

}


//draw a select all question and return how many lines it took up
//@sa      : a pointer to the question 
//@start_y : the starting row to draw the question 
//@return  :the row immediatly after the end of this question
int draw_select_all(select_all *sa,int start_y) {
 
  wattron(pad,COLOR_PAIR(1));
  int i = 0;
  int c = 0;
  int r = 0;
  //question
  while ( i < strlen(sa->prompt)) {
    if ((c % QUESTION_BUFFER_WIDTH) != QUESTION_BUFFER_WIDTH-1 ) {
      mvwaddch(pad,start_y+r,c,(sa->prompt)[i]);
      c++;
    } else {
      c = 0; 
      r++;
    }
    i++;
  }

  r++;  
  c = 0; //place in answer buffer space
  int d = 0; //place in option string
  int option = 0; 
  while (option < sa->num_options) {  //iterate through all options
    while ( d < strlen((sa->options)[option]) -1) { //keep writing characters for the current options till complete
      if ((c % ANSWER_BUFFER_WIDTH) != ANSWER_BUFFER_WIDTH-1 ) {
        mvwaddch(pad,start_y+r,c,((sa->options)[option])[d]);
        //mvwaddch(pad,start_y+r,c,"x");
        c++;
        d++;
      } else {
        c = 0;
        r++;
      }
    }
    //option is complete
    d = 0; 
    c+=OPTION_PAD;//add 4 spaces between options | should be a macro (TBD)
    option++;
    //if next option won't fit on the line add a new line (TBD)
    if ((sa->options)[option]) {
      //handle later (an established max option will be necessary to properly
      //handle this case
    }
  }

  return (r+start_y+1);

}























//need to ban all control/special characters , but
//have a special process for backspace and clear
//WARNING! not checking for buffer overflow
/*
void editField(struct survey_pad *spad,int pad_segment,int selectIndex,char answer[]) {
  //int charCount = 0;
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

  int last = wattron(spad->pad,COLOR_PAIR(1));
  while (ok) {
    mvprintw(20,0,"char pos is %d",charPos); //debug output
    refresh();
    int ic = getch();
    switch(ic) { 
      case 10 :
        ok = 0;
        break;
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
            mvwaddch(spad->pad,QSPACE*(selectIndex),MAX_QCHARS+charPos,ic);
            answer[charPos] = ic;
            answer[charPos+1] = '\0';
            charPos++; 
  
            prefresh(spad->pad,
              pad_segment*spad->vqs*QSPACE,spad->padc_off,
              spad->padr_off,spad->padc_off,
              spad->pad_view_height-2 + spad->padr_off,spad->pad_view_width);
        }
    }
  }
  wattron(spad->pad,COLOR_PAIR(last));
}
*/

/*
void clearField(struct survey_pad *spad,int selectIndex,char answer[]) {
  //clear internal buffer 
  answer[0] = '\0';
  //fill answer field with blank character
  //mvprintw(spad->pad,spad->padr_off+(selectIndex*QSPACE),padc_off+MAX_QCHARS,"");
  int last = wattron(spad->pad,COLOR_PAIR(2));
  for ( int i = 0; i < MAX_QCHARS; i++) {
    mvwaddch(spad->pad,(selectIndex*QSPACE),spad->padc_off+MAX_QCHARS+i,' ');
  }
  wattron(spad->pad,COLOR_PAIR(last));
}
*/


void forceQuit(void) {
  clear();
  refresh();
  endwin();
  fflush(stdout);
  exit(0);
}

//move the pad so that it focuses the ith question from where it was 
//@i question relative to current position
void moveIndex(int i) {
  int new_index = current_question_index + i;
  if (new_index >= 0 && new_index < total_question_count) {
    current_question_index = new_index; 
  }
}

/*
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
        (*pad_segment)--;
      }

    }
 } else {
    if ( *selectIndex != spad->nq-1 ) {

      int before = wattron(spad->pad,COLOR_PAIR(1));
      //delete old cursor 
      mvwaddch(spad->pad->pad,QSPACE*(*selectIndex),0,'>'); 
      wattron(spad->pad,COLOR_PAIR(before));

      if ((*selectIndex) % spad->vqs == 0) {
        (*pad_segment)++;
      }
    }
 }

  prefresh(spad->pad,
    *pad_segment*spad->vqs*QSPACE,spad->padc_off,
    spad->padr_off,spad->padc_off,
    spad->pad_view_height-2 + spad->padr_off,spad->pad_view_width);
}
*/
