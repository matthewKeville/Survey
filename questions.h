//this mixing of pointers and actual types
//seems messy, do people usually keep a consistent method?

typedef struct {
  char *prompt;
  char *response;
} free_response;

//seems like a waste of space for
//selected ...

typedef struct {
  char *prompt;
  char **options;
  int  num_options;
  int  selected;
} multiple_choice;


typedef struct {
  char *prompt;
  char **options;
  int  num_options;
  int  *selected; //int array with num_options ints
} select_all;

typedef struct {
  char *prompt;
  int min;
  int max;
  int step;
  int start;
  int selected;
} scale_choice;


enum Qtype {
  FR = 0,
  MC,
  SA,
  SC  
};

typedef struct {
  enum Qtype type;
  int index;
  int tindex;
  char question_id[6]; //schema id regex states this can be a max of 5, changes to the schema require changes here
} question_header;

