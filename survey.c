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

#define MAX_QCHARS 50   //max chars in question buffer
#define MAX_ACHARS 50   //max chars in answer buffer
#define MAX_QS     30   //max questions
#define QSPACE     5    //row gap between questions
#define BORDER     5    //vertical units of header and footer

#define MAX_FILE_PATH 200
#define MAX_XPATH 200

/*
  I'm unsure what libxml2 does to the DOM elements when I close it.
  Frequently I cast char*'s to memory it has allocated. If the lib
  frees this memory when I close the document. There is no guarentee 
  of what I am pointing to. In the worse case (if it's freed) I will 
  will point to garbage. The best case is that it might not be overwritten
  but this isn't well defined. I will need to read the doc to decide whether 
  I have to allocate different character arrays and copy the value over, 
  or if libxml2 allocation persists after closing.
*/


struct survey_pad {
  WINDOW *pad;
  int padr_off;         //where the pad should start in the window (in rows)
  int padc_off;         //where the pad should start in the window (in cols)
  int pad_view_height;  //viewport height
  int pad_view_width;   //viewport width
  int vqs;              //number of visible questions
  int nq;               //number of questions
};


void editField(struct survey_pad*,int,int,char[]);
void clearField(struct survey_pad*,int,char[]);
void moveIndex(int,int *,int *,struct survey_pad*);
void forceQuit(void);


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
    if (sizeof(survey_file_path) == 0) {
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
    if (xmlStrcmp(cur->name, (const xmlChar *) "survey")) {
      printf("invalid file root node!=survey , has %s", cur->name);
      exit(5);
    }

    // TODO_

    //////////////////////////
    // SCHEMA Validation   //
    ////////////////////////

    char questions[MAX_QS][MAX_QCHARS+1] = {{'\0'}};
    char answers[MAX_QS][MAX_ACHARS+1] = {{'\0'}};
    int nq = 0;

    /////////////////////////
    // PARSE XML
    /////////////////////////

    //How many questions are there?

    xmlXPathContext * xpathCtx;
    xpathCtx = xmlXPathNewContext(doc);

    // Get the id attribute of a node ... (probably not needed)
    // but used below ..
    xmlChar * getIdAttr(xmlNodePtr cur) {
      xmlChar *value;
      if (cur) {
        xmlAttr* id_attr = cur->properties;
          while(id_attr) {
            printf("%s",(char *) id_attr->name);
            if (!xmlStrcmp(id_attr->name,(const xmlChar*)"id")) {
              value = xmlNodeListGetString(cur->doc,id_attr->children,1);
            }
            id_attr=id_attr->next;
          }
      }
      if (!value) {
        printf("getIdAttr returned garbage");
      }
      return value;
    }

    //helper function
    //get the type of a question given an object id
    //xmllint --xpath "//question[@id = 'q2']/questionType/node()" sample.xml
    xmlChar * getQuestionType(xmlChar* id) {
      char idXPath[MAX_XPATH];
      sprintf(idXPath,"//question[@id = '%s']/questionType/node()",(char *) id);
      xmlXPathObjectPtr XPathOP = xmlXPathEvalExpression(idXPath,xpathCtx);
      xmlChar *value  = xmlNodeGetContent((XPathOP->nodesetval->nodeTab[0])); 
      return value;
    }



    ////////////////////////////// FREE RESPONSE PARSER /////////////////////////////////////
    //frs free_response pointer array
    //frc free response count pointer
    //qc question count
    //qhs question header pointer array
    void processFreeResponse(xmlChar *id,free_response **frs,int *frc,question_header **qhs,int *qc) {
      id = (char *) id;
      //extract the prompt
      char questionPromptPath[MAX_XPATH]; 
      sprintf(questionPromptPath,"//question[@id = '%s']/prompt",id);
      printf("%s\n",questionPromptPath);

      xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
      xmlNodeSet *pnodes = pxoptr->nodesetval;
      xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

      char *prompt = malloc( strlen((char *)promptXml) * sizeof (char));
      strcpy(prompt,promptXml);
      printf("prompt %s\n",prompt);

      //package into free_response struct
      free_response * fr_question = malloc(sizeof(free_response));
      fr_question->prompt=prompt;
      //should allow the survey designer the freedom to specify the response length.
      //However it should be constrained by some MACRO max
      fr_question->response = malloc(MAX_ACHARS * sizeof(char)); 
      frs[(*frc)] = fr_question;
  
      //package into question_header
      question_header * q_header = (malloc(sizeof(question_header)));
      q_header->type=FR;
      q_header->index=(*qc);
      q_header->tindex=(*frc);
      strcpy(q_header->question_id,id);
      qhs[*qc] = q_header;   
      printf("frc is %d",*frc);
      (*qc)++;
      (*frc)++;
      

    }


    
    question_header **question_headers = malloc(sizeof(question_header*));
    free_response **free_responses = malloc(sizeof(free_response*));
    select_all **select_alls = malloc(sizeof(select_all*));
    multiple_choice **multiple_choices = malloc(sizeof(multiple_choice*));
    int free_response_index = 0;
    int select_all_index    = 0;
    int multiple_choice_index = 0;
    //int scale_choice_index    = 0;
    int question_index        = 0;



    //Iterate Through all question id's
    const xmlChar* allQuestionIdXPath = "//question/@id";
    xmlXPathObjectPtr XPathOP = xmlXPathEvalExpression(allQuestionIdXPath,xpathCtx);
    xmlNodeSetPtr id_nodes = XPathOP->nodesetval; 
    int question_count = (id_nodes) ? id_nodes->nodeNr : 0;
    printf("found %d questions\n",question_count);
    for (int i = 0; i < question_count; i++) {
      xmlNode *current_id_node = id_nodes->nodeTab[i]; 
      xmlChar *current_id  = xmlNodeGetContent(current_id_node); 
      char *question_type = (char*) getQuestionType(current_id);
      printf("Question ID : %s , Type : %s\n", (char *) current_id,question_type);

      if (strcmp(question_type,"MC")==0) {

      } else if (strcmp(question_type,"SA")==0) {

      
      } else if (strcmp(question_type,"FR")==0) {
        printf("found free response");
        processFreeResponse(current_id,free_responses,&free_response_index,
          question_headers,&question_index); 
      
      } else if (strcmp(question_type,"SC")==0) {

      } else {
        printf("Unkown question type %s, not known to schema",question_type);
        exit(0);
      }

      
    }

    //test that packaging was correct
    printf("mc structure test\n"); 
    for ( int i = 0; i < multiple_choice_index; i++) {
      multiple_choice *mc = multiple_choices[i];
      printf("\n%d %s\n",i,mc->prompt);
      for (int j = 0; j < mc->num_options; j++) {
        printf("%s\t",mc->options[j]);
      }
    }

    //test that packaging was correct
    printf("fr structure test\n"); 
    printf("fr index %d\n",free_response_index);
    for ( int i = 0; i < free_response_index; i++) {
      free_response *fr = free_responses[i];
      printf("\n%s\n",fr->prompt);
    }

    printf("sa structure test\n"); 
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

    printf("\n question header test \n");
    for ( int i = 0; i < question_index; i++) {
      question_header *qh = question_headers[i];
      printf("\nType %d\n",qh->type);
      printf("\nQuestion Index %d\n",qh->index);
      printf("\nType Index %d\n",qh->tindex);
      printf("\nID%s\n",qh->question_id);
    }



    
    exit(0);




    //REFACTOR BELOW TO FUNCTION LIKE ABOVE FOR MC

    /*
    //////////////////////// MULTIPLE CHOICE////////////////////////////////////

    //How many multiple-choice questions are there?
    const xmlChar* multipleChoicePath = "//question/multipleChoice/exclusive"
      "[text() = '1']/ancestor::question";  //string literal continuation syntax
    xoptr = xmlXPathEvalExpression(multipleChoicePath,xpathCtx);
    nodes = xoptr->nodesetval; 
    size = (nodes) ? nodes->nodeNr : 0;

    multiple_choice **multiple_choices = malloc(sizeof(multiple_choice*));
    printf("found %d multiple chocie\n",size);


    //loop through each multiple choice question 
    if (size != 0) {
      for ( int i = 0; i < size; i++) {
        cur = nodes->nodeTab[i]; //mc node set
        xmlChar * xid = getIdAttr(cur);  
        if(!xid || strlen((char *) xid)==0) {
          printf("id not found\n");
          exit(98);
        }

        char *id = (char *) xid;
        printf("found id %s\n" , id);

        //extract question info 
        char multipleChoiceAnswerPath[MAX_XPATH]; //should replace with maxquery macro
        sprintf(multipleChoiceAnswerPath,"//question[@id = '%s']/multipleChoice/answer",(char *) xid);
        printf("%s\n",multipleChoiceAnswerPath);

        xmlXPathObject * axoptr = xmlXPathEvalExpression(multipleChoiceAnswerPath,xpathCtx);
        xmlNodeSet *anodes = axoptr->nodesetval; 
        int options_size = (anodes) ? anodes->nodeNr : 0;
        
        //options
        char **optionSet = malloc(options_size * sizeof(char *));
        printf("%d answers found\n",options_size);
        for ( int j = 0; j < options_size; j++) {
          xmlNode *dur = anodes->nodeTab[j];
          char *value  = (char *)xmlNodeGetContent(dur); //shortcut for going to child text and getting its content
          printf("found option  %s\n",value);
          int optionLength = (int) strlen(value);
          printf("length %ld\n",strlen(value));
          optionSet[j] = malloc( (optionLength+1) * sizeof(char));
          strcpy(optionSet[j],value);
        }

        //extract the prompt
        char questionPromptPath[MAX_XPATH]; 
        sprintf(questionPromptPath,"//question[@id = '%s']/prompt",(char *) xid);
        printf("%s\n",questionPromptPath);

        xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
        xmlNodeSet *pnodes = pxoptr->nodesetval;
        xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

        char *prompt = malloc( strlen((char *)promptXml) * sizeof (char));
        strcpy(prompt,promptXml);
        printf("prompt %s\n",prompt);

        //package into multiple_choice struct
        multiple_choice * mc_question = malloc(sizeof(multiple_choice));
        mc_question->prompt=prompt;
        mc_question->options=optionSet; 
        mc_question->num_options=options_size;
        mc_question->selected=-1;
        multiple_choices[i] = mc_question;
    
        //package into question_header
        question_header * q_header = (malloc(sizeof(question_header)));
        q_header->type=MC;
        q_header->index=current_question;
        q_header->tindex=i;
        strcpy(q_header->question_id,id);
        question_headers[i] = q_header;   
        
        
        //increment total question count 
        current_question++; 
      }
    }
    */



    //////////////////////// SELECT ALL ////////////////////////////////////
    /*
    //How many select all questions are there?
    const xmlChar* selectAllPath = "//question/multipleChoice/exclusive"
      "[text() = '0']/ancestor::question";  //string literal continuation syntax
    xoptr = xmlXPathEvalExpression(selectAllPath,xpathCtx);
    nodes = xoptr->nodesetval; 
    size = (nodes) ? nodes->nodeNr : 0;

    select_all **select_alls = malloc(sizeof(select_all*));
    printf("found %d select all\n",size);


    //loop through each select all question 
    if (size != 0) {
      for ( int i = 0; i < size; i++) {
        cur = nodes->nodeTab[i]; //sa node set
        xmlChar * xid = getIdAttr(cur);  
        if(!xid || strlen((char *) xid)==0) {
          printf("id not found\n");
          exit(98);
        }

        char *id = (char *) xid;
        printf("found id %s\n" , id);

        //extract question info 
        char selectAllAnswerPath[MAX_XPATH]; //should replace with maxquery macro
        sprintf(selectAllAnswerPath,"//question[@id = '%s']/multipleChoice/answer",(char *) xid);
        printf("%s\n",selectAllAnswerPath);

        xmlXPathObject * axoptr = xmlXPathEvalExpression(selectAllAnswerPath,xpathCtx);
        xmlNodeSet *anodes = axoptr->nodesetval; 
        int options_size = (anodes) ? anodes->nodeNr : 0;
        
        //options
        char **optionSet = malloc(options_size * sizeof(char *));
        printf("%d answers found\n",options_size);
        for ( int j = 0; j < options_size; j++) {
          xmlNode *dur = anodes->nodeTab[j];
          char *value  = (char *)xmlNodeGetContent(dur); //shortcut for going to child text and getting its content
          printf("found option  %s\n",value);
          int optionLength = (int) strlen(value);
          printf("length %ld\n",strlen(value));
          optionSet[j] = malloc( (optionLength+1) * sizeof(char));
          strcpy(optionSet[j],value);
        }

        //extract the prompt
        char questionPromptPath[MAX_XPATH]; 
        sprintf(questionPromptPath,"//question[@id = '%s']/prompt",(char *) xid);
        printf("%s\n",questionPromptPath);

        xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
        xmlNodeSet *pnodes = pxoptr->nodesetval;
        xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

        char *prompt = malloc( strlen((char *)promptXml) * sizeof (char));
        strcpy(prompt,promptXml);
        printf("prompt %s\n",prompt);

        //package into select all struct
        select_all * sa_question = malloc(sizeof(select_all));
        sa_question->prompt=prompt;
        sa_question->options=optionSet; 
        sa_question->num_options=options_size;
        sa_question->selected=calloc(options_size,sizeof(int));
        select_alls[i] = sa_question;
    
        //package into question_header
        question_header * q_header = malloc(sizeof(question_header)); 
        q_header->type=SA;
        q_header->index=current_question;
        q_header->tindex=i;
        strcpy(q_header->question_id,id);
        question_headers[i] = q_header;   
        
        
        //increment total question count 
        current_question++; 
      }
    }
    */





    
    ////////////////////////////// FREE RESPONSE /////////////////////////////////////
    /* 
    //How many free response questions are there?
    const xmlChar* freeResponsePath = "//question/freeResponse/ancestor::question";
    xoptr = xmlXPathEvalExpression(freeResponsePath,xpathCtx);
    nodes = xoptr->nodesetval; 
    size = (nodes) ? nodes->nodeNr : 0;

    free_response **free_responses = malloc(sizeof(free_response*));
    printf("found %d free response\n",size);


    //loop through each free response question 
    if (size != 0) {
      for ( int i = 0; i < size; i++) {
        cur = nodes->nodeTab[i]; //fr node set
        xmlChar * xid = getIdAttr(cur);  
        if(!xid || strlen((char *) xid)==0) {
          printf("id not found\n");
          exit(98);
        }

        char *id = (char *) xid;
        printf("found id %s\n" , id);

        //extract the prompt
        char questionPromptPath[MAX_XPATH]; 
        sprintf(questionPromptPath,"//question[@id = '%s']/prompt",(char *) xid);
        printf("%s\n",questionPromptPath);

        xmlXPathObject * pxoptr = xmlXPathEvalExpression(questionPromptPath,xpathCtx);
        xmlNodeSet *pnodes = pxoptr->nodesetval;
        xmlChar *promptXml = xmlNodeGetContent(pnodes->nodeTab[0]);

        char *prompt = malloc( strlen((char *)promptXml) * sizeof (char));
        strcpy(prompt,promptXml);
        printf("prompt %s\n",prompt);

        //package into free_response struct
        free_response * fr_question = malloc(sizeof(free_response));
        fr_question->prompt=prompt;
        //should allow the survey designer the freedom to specify the response length.
        //However it should be constrained by some MACRO max
        fr_question->response = malloc(MAX_ACHARS * sizeof(char)); 
        free_responses[i] = fr_question;
    
        //package into question_header
        question_header * q_header = (malloc(sizeof(question_header)));
        q_header->type=FR;
        q_header->index=current_question;
        q_header->tindex=i;
        strcpy(q_header->question_id,id);
        question_headers[i] = q_header;   
        
        
        //increment total question count 
        current_question++; 
      }
    }
  */















    ////////////////
    //SA 
    /*
    const xmlChar* selectAllPath = "//question/multipleChoice/exclusive[text() = '0']/ancestor::question";
    xoptr = xmlXPathEvalExpression(selectAllPath,xpathCtx);
    nodes = xoptr->nodesetval; 
    size = (nodes) ? nodes->nodeNr : 0;
    select_all *select_alls = malloc(size * sizeof(select_all));
    printf("found %d select all\n",size);
    */
   

    ///////////////
    //SC
    /*
    const xmlChar* scaleChoicePath = "//question/scaleChoice/ancestor::question";
    xoptr = xmlXPathEvalExpression(scaleChoicePath,xpathCtx);
    nodes = xoptr->nodesetval; 
    size = (nodes) ? nodes->nodeNr : 0;
    scale_choice *scale_choices = malloc(size * sizeof(scale_choice));
    printf("found %d scale choice\n",size);
    */
   

    


    //This will free the entire document tree in memory, so any 
    //pointers to it will become unreliable
    //it's probably best xmlFreeDoc when the dom has been parsed, but
    //for now i have too many pointers to it, it's a refactor for the future
    //xmlFreeDoc(doc);

    exit(0);


    //get time info
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
    int lines = 36;
    int cols  = 120;
    //index of end line and col
    int maxl = lines-1;
    int maxc = cols-1;
    wresize(stdscr,lines,cols);  //guess of supported screen size (based on vim lns)
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

    /////////////////////////
    //    Window Layout
    /////////////////////////
 
    char submit_key_string[] = "F1";  //Survey submission key
 
    //Write Survey Banner
    addstr("Daily Survey\n");
    //Submission info
    printw("PRESS %s , to submit | PRESS ENTER , to type,  PRESS BACKSPACE , to clear field",submit_key_string);
    refresh(); 

    /////////////////////////
    //  Generate Survey Pad
    /////////////////////////

    attron(COLOR_PAIR(1));

    //calculate the size of the padd that fits between the borders
    int padspacel = MAX_QS*(QSPACE+1);
    if (padspacel < maxl - (2*BORDER)) {
      padspacel = maxl - ( 2*BORDER);
    }

    //demarcate pad/window boundary
    mvaddch(maxl-1,maxc-1,'$');
    mvaddch(maxl-1,0,'$');

    WINDOW *pad = newpad(padspacel,maxc);
    //generate survey in pad
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
   
    int padr_off = BORDER; //what row does the pad start
    int padc_off = 0;
    int pad_view_height = maxl - (2*BORDER); //how many rows does it go for/
    int pad_view_width  = maxc;

    //calculate the number of questions that will fit in the pad viewport
    int vqs = pad_view_height/QSPACE; // +1
    int pad_segment = 0; //what segement of the pad we are on 

    //create a survey_pad struct to group pad dimensions and state
    struct survey_pad spad = {
      .pad             = pad,
      .padr_off        = padr_off,
      .padc_off        = padc_off,
      .pad_view_height = pad_view_height,
      .pad_view_width  = pad_view_width,
      .vqs             = vqs,
      .nq              = nq
    };

    //DEBUG BUFFER
    int dby = BORDER+pad_view_height+1;
    int dbx = 0;
    mvprintw(dby,0,"vqs is %d",vqs); //debug output

    //DEBUG PAD BORDER
    mvprintw(padr_off-1,0,"-------PAD BORDER-------"); //just before padd starts
    mvprintw(padr_off+pad_view_height,0,"------PAD BORDER--------");//just after pad ends


    //draw initial question selector
    int selectIndex = 0;
    int before = wattron(pad,COLOR_PAIR(1));
    mvwaddch(pad,QSPACE*selectIndex,0,'>'); 
    wattron(pad,COLOR_PAIR(before));
    prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,padc_off,pad_view_height-2+padr_off,pad_view_width);


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
          clearField(&spad,selectIndex,answers[selectIndex]);

          prefresh(pad,(pad_segment)*(vqs*QSPACE),0,padr_off,padc_off,pad_view_height-2+padr_off,pad_view_width);

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

    //validate that all questions have been answered
    int complete = 1;
    for ( int i = 0; i < nq; i++ ) {
      complete &= ( answers[i][0] != '\0');
    }

    if (!complete) {
      ok = 1;
      goto USERREAD; 
    }

    //get elapsed time
    struct timeval t2;
    gettimeofday(&t2,NULL);
    printf("elapsed time is %ld \n",(t2.tv_sec)-(t1.tv_sec));

   
    //clean up ncurses 
    clear();
    refresh();
    endwin();

    //print answers to standard out
    /*
    for ( int i = 0; i < nq; i++ ) {
      printf("%d  %s\n",i,answers[i]);
    }
    */
    fflush(stdout);
    
    //save answers to file 
    char *home = getenv("HOME");
    if (home == NULL) {
      printf("unable to find user home");
      exit(9);
    }

    char *fileName = "/save.xml";
    int pathLength = strlen(home) + strlen(fileName)-1;
    char path[pathLength];

    strncpy(path,home,strlen(home));
    strncat(path,fileName,strlen(fileName));

    printf("opening %s for writing\n",path);
    FILE *fprt; //should be checking if this is a valid pointer
    if (fprt = fopen(path,"w")) {
      for ( int i = 0; i < nq; i++) {
        fprintf(fprt,"%s\n",answers[i]);
      } 
    } else {
      printf("Unable to save survey");
      exit(10);
    }
    fclose(fprt); 


    return EXIT_SUCCESS;
}


//need to ban all control/special characters , but
//have a special process for backspace and clear
//WARNING! not checking for buffer overflow
void editField(struct survey_pad *spad,int pad_segment,int selectIndex,char answer[]) {
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

  int last = wattron(spad->pad,COLOR_PAIR(1));
  while (ok) {
    mvprintw(20,0,"char pos is %d",charPos); //debug output
    refresh();
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
  
            prefresh(spad->pad,
              pad_segment*spad->vqs*QSPACE,spad->padc_off,
              spad->padr_off,spad->padc_off,
              spad->pad_view_height-2 + spad->padr_off,spad->pad_view_width);
        }
    }
  }
  wattron(spad->pad,COLOR_PAIR(last));
}

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


void clearField(struct survey_pad *spad,int selectIndex,char answer[]) {
  //clear internal buffer 
  answer[0] = '\0';
  //fill answer field with blank character
  //mvprintw(spad->pad,spad->padr_off+(selectIndex*QSPACE),padc_off+MAX_QCHARS,"");
  int last = wattron(spad->pad,COLOR_PAIR(2));
  for ( int i = 0; i < MAX_QCHARS; i++) {
    mvwaddch(spad->pad,(selectIndex*QSPACE),spad->padc_off+MAX_QCHARS+i,' ');
  }
  wattron(spad->pad,COLOR_PAIR(1));
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
        (*pad_segment)++;
      }
    }
 }

  prefresh(spad->pad,
    *pad_segment*spad->vqs*QSPACE,spad->padc_off,
    spad->padr_off,spad->padc_off,
    spad->pad_view_height-2 + spad->padr_off,spad->pad_view_width);
}
