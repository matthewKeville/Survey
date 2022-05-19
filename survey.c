#include<ncurses.h>
#include<stdlib.h>
#define MAX_QCHARS 50
//--------------------------------------------------------
// FUNCTION PROTOTYPES
//--------------------------------------------------------
void printing();
void moving_and_sleeping();
void colouring();

//--------------------------------------------------------
// FUNCTION main
//--------------------------------------------------------
int main(void)
{
    initscr(); // this starts ncurses
    wresize(stdscr,26,120); //guess of supported screen size (based on vim lns)
    noecho();  // disable echo
    raw();     // disable line buffering
    keypad(stdscr,TRUE); //Get access to F1,F2.... for our window (stdscr)

    if (!has_colors()) {
      addstr("your terminal does not support colors, aborting");
      refresh();
    }

    if (start_color() != OK) {
      addstr("Unable to start colors, aborting");
      refresh();
      exit(1);
    }

    // one of the ncurses printing methods (outputs to a buffer)
    addstr("Daily Survey");
    refresh();

    // dummy questions set
    int  nq = 4;
    char questions[4][MAX_QCHARS] = {
      {" What did you have for breakfast?"},
      {" Did you bring lunch today?      "},
      {" How did you sleep?              "},
      {" What is your mood?              "}
    };

    //Print out all questions
    init_pair(1,COLOR_MAGENTA,COLOR_BLACK); //questions
    init_pair(2,COLOR_BLUE,COLOR_BLUE);     //answer field (blocked)
    init_pair(3,COLOR_WHITE,COLOR_GREEN); //submit text
    attron(COLOR_PAIR(1));
    
    int sy,sx;
    getyx(stdscr,sy,sx);  
    //Print question and response fields
    for ( int i = 0; i < nq; i++) {
      printw("\n\n    [%d]  %s",i,questions[i]);
      int y,x;
      getyx(stdscr,y,x);
      move(y,MAX_QCHARS);
      attron(COLOR_PAIR(2));
      printw("xxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyy");
      attron(COLOR_PAIR(1));
    }
    //Print Submit Button
    printw("\n\n");
    attron(COLOR_PAIR(3));
    printw(" Submit ");
    attron(COLOR_PAIR(1));
    refresh();
  
    int selectIndex = 0;
    move(2+sy+(2*selectIndex),0);
    printw(">");
    refresh();
    //draw a cursor at the selected index entry 


    //read in keypresses until 'q' is pressed
    /*
    int uch;
    int ok = 1;
    int isBold = 0;
    while (ok) {
      uch = getch();
      addstr("\nyou entered ");
      isBold = (isBold + 1) % 2;
      if (isBold) {
      attron(A_BOLD);
      }
      //addch(uch);
      printw("%c",uch);
      attroff(A_BOLD);
      if (uch == KEY_F(1)) {
        printw("F1 , you sneaky bastard");
      }
      refresh();
      ok = ( uch != 113 ); //if user enters q exit
    }
    */

    getch();



    //end ncurses before the program exits
    endwin();

    return EXIT_SUCCESS;
}

//--------------------------------------------------------
// FUNCTION printing
//--------------------------------------------------------
void printing()
{

    // addstr - prints specified string           | stdio puts
    // addch  - prints a single character         | stdio putchar
    // printw - the ncurses equivalent of printf  | stdio printf
    //* except you need to call refersh to copy from the buffer to the screen 

    addstr("This was printed using addstr\n\n");
    refresh();

    addstr("The following letter was printed using addch:- ");
    addch('a');
    refresh();

    printw("\n\nThese numbers were printed using printw\n%d\n%f\n", 123, 456.789);
    refresh();
}

//--------------------------------------------------------
// FUNCTION moving_and_sleeping
//--------------------------------------------------------
void moving_and_sleeping()
{
    int row = 5;
    int col = 0;

    curs_set(0);

    for(char c = 65; c <= 90; c++)
    {
        move(row++, col++);
        addch(c);
        refresh();
        napms(100);
    }

    row = 5;
    col = 3;

    for(char c = 97; c <= 122; c++)
    {
        mvaddch(row++, col++, c);
        refresh();
        napms(100);
    }

    curs_set(1);

    addch('\n');
}

//--------------------------------------------------------
// FUNCTION colouring
//--------------------------------------------------------
void colouring()
{
    if(has_colors())  //does this terminal support colors?
    {
        if(start_color() == OK) 
        {
            //all color constants { BLACK , RED , GREEN , YELLOW , BLUE , MAGENTA , CYAN , WHITE }
            init_pair(1, COLOR_YELLOW, COLOR_RED); //establish fg / bg color pairs
            init_pair(2, COLOR_GREEN, COLOR_GREEN);
            init_pair(3, COLOR_MAGENTA, COLOR_CYAN);

            attrset(COLOR_PAIR(1));  //set printing attributes
            addstr("Yellow and red\n\n");
            refresh();
            attroff(COLOR_PAIR(1));

            attrset(COLOR_PAIR(2) | A_BOLD); //set printing attributes ( OR's with bold makes foregroudn brighter to differentiate green on green
            addstr("Green and green A_BOLD\n\n");
            refresh();
            attroff(COLOR_PAIR(2));
            attroff(A_BOLD);

            attrset(COLOR_PAIR(3));
            addstr("Magenta and cyan\n");
            refresh();
            attroff(COLOR_PAIR(3));
        }
        else
        {
            addstr("Cannot start colours\n");
            refresh();
        }
    }
    else
    {
        addstr("Not colour capable\n");
        refresh();
    }
} 
