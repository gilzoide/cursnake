// Compile with '-lpanel -lncurses'

#include <ncurses.h>
#include <panel.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define BGhud 1
#define FGgreen 2
#define FGred 3
#define FGyellow 4
#define BGhelp 5
#define BGboom 6

#define HEIGHT 22
#define WIDTH 60
#define HELP_WIDTH 52
#define FIELD_y0 ((LINES - HEIGHT)/2)
#define FIELD_x0 ((COLS - WIDTH)/2)
#define SNAKE_y0 (HEIGHT/2)
#define SNAKE_x0 (WIDTH/2 - 2)
/*
   <==\			VV
      H			/
      H			\==>
      H    <
      \===/<
				/==\
A				H  H
H				H  \
\==\<			V  AA
   \<

two-headed 'snake style'
*/

typedef struct body_t {
	char type;	// depends on relative position in the snake; may be any of the chars seen in 'snake style'
	int y;	// snake
	int x;	// position
	struct body_t *next;
}BODY;	// only need to store the tail one, and create more as food is eaten

typedef struct head_t {
	char part[2];
	char side;	// what side is second head?	[Right, Left]
	char mov;	// where's snake going to?	[Up, Down, Left, Right]
	int y;	// main head's
	int x;	// position
	int appendix_x;	// 2nd head's
	int appendix_y;	// position
	struct body_t *neck;	// maybe we'd like to know where the neck is, so...
}HEAD;	// part[0] being the normal head and part[1] being the side one


int score = 0;
char dificulty = 0;	// speed and stones; -1 means "game over"
char turned = 0;	// 1 if turned, 2 if right after turning, 0 normal
char corner;	// if turned, store the type of the corner
int live_y, live_x;
WINDOW *hud;


/* Displays the help (in a created window and panel, for going back to the normal field after) */
void Help ()
{
	WINDOW *help;
	PANEL *up;

	help = newwin (12, HELP_WIDTH, 1, 0);
	up = new_panel (help);
	update_panels ();
	doupdate ();

	box (help, 0, 0);
	wbkgd (help, COLOR_PAIR (BGhelp));
	wrefresh (help);
	mvwaddstr (help, 0, HELP_WIDTH/2 - 2, "HELP");
	wattron (help, A_BOLD);
	mvwaddstr (help, 1, 1, "Arrow Keys or W,A,S,D:");
	mvwaddstr (help, 2, 1, "'z':");
	mvwaddstr (help, 3, 1, "Space:");
	mvwaddstr (help, 4, 1, "'q':");

	mvwaddstr (help, 6, 1, "O:");
	mvwaddstr (help, 7, 1, "/VC");
	mvwaddstr (help, 8, 1, "\\_/:");

	mvwaddstr (help, 10, 1, "K:");

	wattrset (help, COLOR_PAIR (BGhelp));
	mvwaddstr (help, 1, 24, "turn");
	mvwaddstr (help, 2, 6, "change second head's side");
	mvwaddstr (help, 3, 8, "pause");
	mvwaddstr (help, 4, 6, "quit");
	mvwaddstr (help, 6, 4, "food: eat it for some points");
	mvwaddstr (help, 8, 6, "apple: eat it with both heads for 500 points!");
	mvwaddstr (help, 10, 4, "K stands for Hunter: it'll try to kill you!");
// but actually you need to spend 500 - (dificulty + 1) points in it, so is it worth it? xP


// writes the help window, wait for some key to be pressed and delete the help window
	wrefresh (help);
	nodelay (stdscr, FALSE);
	getch ();
	nodelay (stdscr, TRUE);

	wbkgd (help, COLOR_PAIR (0));
	werase (help);
	wrefresh (help);
	del_panel (up);
	delwin (help);
}


/* Rewrite the score in hud */
void ReHud ()
{
	mvwprintw (hud, 0, COLS - 12, "score: %5d", score);
	wrefresh (hud);
}


/* If crossed any boundaries, go to other side */
void Boundary (int *y, int *x)
{
	if (*y <= 0) {
		*y = *y + HEIGHT - 2;
	}
	else if (*y >= HEIGHT - 1) {
		*y = *y - HEIGHT + 2;
	}
	else if (*x <= 0) {
		*x = *x + WIDTH - 2;
	}
	else if (*x >= WIDTH - 1) {
		*x = *x - WIDTH + 2;
	}
}


/* Create the field (oh, really) */
WINDOW *CreateField (char dificulty)
{
	WINDOW *win;
	int i, y, x, c;

	win = subwin (stdscr, HEIGHT, WIDTH, FIELD_y0, FIELD_x0);
	wattron (win, A_BOLD);
	box (win, 0, 0);

// stones!
	for (i = 0; i <= dificulty; i++) {
		do {
			x = (rand () % (WIDTH - 2)) + 1;
			y = (rand () % (HEIGHT - 2)) + 1;
			c = mvwinch (win, y, x);
		} while (c == '@' || (x >= SNAKE_x0 && x <= SNAKE_x0 + 5 && (y == SNAKE_y0 || y == SNAKE_y0 - 1)));
		mvwaddch (win, y, x, '@');
	}

	live_y = y;
	live_x = x;
	wattron (win, COLOR_PAIR (FGyellow) | A_BOLD);
	mvwaddch (win, y, x, 'K');

	wrefresh (win);

	return win;
}


/* Create the snake, with tail, neck and 1 head (and return tail to main) */
BODY *CreateSnake (WINDOW *field, HEAD *head)
{
	BODY *tail;

	if ((tail = (BODY*) malloc (sizeof (BODY))) == NULL)
		exit (1);
	if ((tail->next = (BODY*) malloc (sizeof (BODY))) == NULL)
		exit (1);

	wattrset (field, COLOR_PAIR (FGgreen));

	tail->y = SNAKE_y0;
	tail->x = SNAKE_x0;
	tail->type = '<';
	mvwaddch (field, tail->y, tail->x, tail->type);

	tail->next->y = tail->y;
	tail->next->x = tail->x + 1;
	tail->next->type = '/';
	tail->next->next = NULL;
	mvwaddch (field, tail->next->y, tail->next->x, tail->next->type);

// heads
	head->part[0] = '<';
	head->part[1] = '<';
	head->side = 'L';
	head->mov = 'R';
	head->neck = tail->next;
// heads' position
	head->y = head->neck->y;
	head->x = head->neck->x + 1;
	head->appendix_y = head->y - 1;
	head->appendix_x = head->x;
// now draw it xD
	wattron (field, A_BOLD);

	mvwaddch (field, head->y, head->x, head->part[0]);
	mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);

	wattroff (field, A_BOLD);

	wrefresh (field);

	return tail;
}


/* Anyone else's hungry? */
void ThrowFood (WINDOW *field)
{
	int y, x, c;

	wattron (field, COLOR_PAIR (FGred) | A_BOLD);

	do {
		x = (rand () % (WIDTH - 2)) + 1;
		y = (rand () % (HEIGHT - 2)) + 1;
		c = mvwinch (field, y, x);
	} while (c != ' ');
	mvwaddch (field, y, x, 'O');

	wrefresh (field);

	wattrset (field, COLOR_PAIR (FGgreen));
}


/* See if beat the Hi Score, if yes: write it, else: show the 3 firsts */
void HiScore ()
{
	FILE *f;
	char names[3][19] = {"", "", ""};	// names of the hiscorers
	int hi[3] = {0, 0, 0}, i;	// hiscores
	char new[19];	// name of the new hiscorer

	if ((f = fopen ("HiScore", "r+")) == NULL) {
		fclose (f);
		f = fopen ("HiScore", "a");
	}

// get the names and hiscores from the HiScore file
	while (!feof (f)) {
		for (i = 0; i < 3; i++) {
			fread (names[i], sizeof (char), 19, f);
			fread (&hi[i], sizeof (int), 1, f);
		}
	}

	flushinp ();
	attrset (COLOR_PAIR (0));
	nodelay (stdscr, FALSE);

// player achieved a Hi Score!
	if (score > hi[2]) {
		echo ();
		nocbreak ();
		curs_set (1);

		attron (A_BOLD);
		mvaddstr (FIELD_y0 + HEIGHT, COLS/2 - 25, "New HiScore! Your name, please > ");
		attroff (A_BOLD);
		getnstr (new, 18);
		new[18] = 0;

// n° 1
		if (score > hi[0]) {
			fseek (f, 0, SEEK_SET);
// rewrite the file with the new entry, and push the other ones
			fwrite (new, sizeof (char), 19, f);
			fwrite (&score, sizeof (int), 1, f);

			fwrite (names[0], sizeof (char), 19, f);
			fwrite (&hi[0], sizeof (int), 1, f);

			fwrite (names[1], sizeof (char), 19, f);
			fwrite (&hi[1], sizeof (int), 1, f);
// rewrite the values
			strcpy (names[2], names[1]);
			hi[2] = hi[1];
			strcpy (names[1], names[0]);
			hi[1] = hi[0];
			strcpy (names[0], new);
			hi[0] = score;
		}
// n° 2
		else if (score > hi[1]) {
			fseek (f, (19 * sizeof (char) + sizeof (int)), SEEK_SET);
// rewrite the file with the new entry [leave n°1 intact], and push the other ones
			fwrite (new, sizeof (char), 19, f);
			fwrite (&score, sizeof (int), 1, f);

			fwrite (names[1], sizeof (char), 19, f);
			fwrite (&hi[1], sizeof (int), 1, f);
// rewrite the values
			strcpy (names[2], names[1]);
			hi[2] = hi[1];
			strcpy (names[1], new);
			hi[1] = score;
		}
// n° 3
		else {
			fseek (f, 2 * (19 * sizeof (char) + sizeof (int)), SEEK_SET);
// rewrite the file with the new entry [leave n°1 and n°2 intact]
			fwrite (new, sizeof (char), 19, f);
			fwrite (&score, sizeof (int), 1, f);
// rewrite the last value
			strcpy (names[2], new);
			hi[2] = score;
		}

		move (FIELD_y0 + HEIGHT, 0);
		clrtoeol ();

		curs_set (0);
		cbreak ();
		noecho ();
	}

	fclose (f);

	attron (A_BOLD);
	mvaddstr (FIELD_y0 + HEIGHT - 1, COLS/2 - 5, " HI SCORE ");
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 - 40, "1.", names[0], hi[0]);
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 - 12, "2.", names[1], hi[1]);
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 + 16, "3.", names[2], hi[2]);

	attroff (A_BOLD);
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 - 38, "%s %d", names[0], hi[0]);
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 - 10, "%s %d", names[1], hi[1]);
	mvprintw (FIELD_y0 + HEIGHT, COLS/2 + 18, "%s %d", names[2], hi[2]);

	refresh ();

	getch ();
	nodelay (stdscr, TRUE);
}



/* You lost, sucka, sorry =P */
void Loser (WINDOW *field, HEAD *head, BODY *tail)
{
	BODY *aux = tail;
	int i = 0;

	mvwaddch (field, head->y, head->x, head->part[0]);
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);

	wrefresh (field);

// redify every body part, starting from tail [then blackify it]
	while (aux->next != NULL) {
		wattron (field, A_BOLD | COLOR_PAIR (FGred));
		mvwaddch (field, tail->y, tail->x, tail->type);
		wrefresh (field);
		usleep (3e5);
		wattroff (field, A_BOLD);
		mvwaddch (field, tail->y, tail->x, tail->type);
		if (i > 0)
			mvwaddch (field, aux->y, aux->x, ' ');
		aux = tail;
		tail = tail->next;
		i++;
	}

// heads blinking in red
	wattron (field, COLOR_PAIR (FGred));
	for (i = 0; i < 10; i++) {
		wattron (field, A_BOLD);
		mvwaddch (field, head->y, head->x, head->part[0]);
		if (head->part[1])
			mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);
		wrefresh (field);
		usleep (1e5);

		wattroff (field, A_BOLD);
		mvwaddch (field, head->y, head->x, head->part[0]);
		if (head->part[1])
			mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);
		wrefresh (field);
		usleep (1e5);
	}

// explode head[s]! VWAHAHAHAHA xD
	wattron (field, COLOR_PAIR (BGboom) | A_BOLD);
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, ' ');

// let's be certain "BOOM" will be within the field =]
	if (head->x == 1)
		head->x = 2;
	else if (head->x > WIDTH - 3)
		head->x = WIDTH - 4;

	mvwaddstr (field, head->y - 1, head->x - 2, "\\/\\/\\/");
	mvwaddstr (field, head->y, head->x - 2, "-BOOM-");
	mvwaddstr (field, head->y + 1, head->x - 2, "/\\/\\/\\");

// some special effects [fake blood]
	for (i = 0; i < 40; i++) {
		mvwaddch (field, rand () % HEIGHT, rand () % WIDTH, ' ');
		usleep (5e4);
		wrefresh (field);
	}

	usleep (1e6);

	HiScore ();

	dificulty = -1;
}


/* Ah, you lost 2nd head! Now your game sucks, please restart, ok? */
void OneHead (WINDOW *field, HEAD *head)
{
	int i;
	char *cries[] = {
		"I'm sorry, 2nd head =/",
		"NOOOOOOOOOOOO!",
		"My life will never be the same again...",
		"2ndy, I always loved you! Don't leave me!",
		"He's in a better place now...",

		"He kicked the bucket. That must hurt the teeth, huh =P",
		"Don't even need'ya, loser",
		"In your face! HAHA",
		"Pfs, who let that sucker play anyway?",
		"What U waitin for, let's go, yo!",

		"Can't believe I used to talk to this useless --'",
		"Only one explanation: human in control, *tsc tsc*",
		"You should restart, seriously, 2 heads think better than one!",
		"Dude, you retarded?",
		"Oh maaaan! Tottally ain't my fault!"
	};

	attrset (COLOR_PAIR (0));

	i = rand () % 15;
	mvaddch (FIELD_y0 + HEIGHT, COLS/2 - strlen (cries[i])/2 - 11, '"');
	addstr (cries[i]);
	addch ('"');

	attron (A_BOLD);
	addstr (" click to continue");
	refresh ();

	nodelay (stdscr, FALSE);
	getch ();
	nodelay (stdscr, TRUE);

	move (FIELD_y0 + HEIGHT, 0);
	clrtoeol ();
	refresh ();

	head->part[1] = 0;
}


/* Ate something, grow 1 BODY */
BODY *Grow (WINDOW *field, BODY *tail)
{
	BODY *aux;

	if ((aux = (BODY*) malloc (sizeof (BODY))) == NULL)
		exit (1);

// double the tail, so that when it moves, there's one more, so it's like growing behind
	aux->x = tail->x;
	aux->y = tail->y;
	aux->type = tail->type;

	aux->next = tail;

// a new food must be set in the game
	ThrowFood (field);

	score += (dificulty + 1);
	ReHud ();

	return aux;
}


/* Check collision → 0 for nothing, 1 for deadly hit, 2 for eating, -1 for losing 2nd head */
int Hit (WINDOW *field, char c_head, char c_appendix)
{
// 2 heads
	if (c_appendix) {
		switch (c_head) {
			case '/': case '\\': case '=': case 'H': case '>': case '<': case 'A': case '@': case 'K':
				return 1;

			case 'O':
				return 2;
// if it's switching 2nd head's side, just check 2nd head's hits
			case 0:
				break;
		}
		switch (c_appendix) {
			case '/': case '\\': case '=': case 'H': case '>': case '<': case 'A': case '@': case 'K':
				return -1;

			case 'O':
				return 2;
		}
	}
// only one head
	else {
		switch (c_head) {
			case '/': case '\\': case '=': case 'H': case '>': case '<': case 'A': case '@': case 'K':
				return 1;

			case 'O':
				return 2;
		}
	}
}


/* Switch 2nd head's side */
void Switch (WINDOW *field, HEAD *head, BODY *tail)
{
	char c_appendix;

	mvwaddch (field, head->appendix_y, head->appendix_x, ' ');

// change 2nd head's position
	if ((head->mov == 'U' && head->side == 'L') || (head->mov == 'D' && head->side == 'R')) {
		head->appendix_x += 2;
		head->neck->type = '/';
	}
	else if ((head->mov == 'U' && head->side == 'R') || (head->mov == 'D' && head->side == 'L')) {
		head->appendix_x -= 2;
		head->neck->type = '\\';
	}
	else if ((head->mov == 'L' && head->side == 'L') || (head->mov == 'R' && head->side == 'R')) {
		head->appendix_y -= 2;
		head->neck->type = '/';
	}
	else if ((head->mov == 'L' && head->side == 'R') || (head->mov == 'R' && head->side == 'L')) {
		head->appendix_y += 2;
		head->neck->type = '\\';
	}

	Boundary (&head->appendix_y, &head->appendix_x);

	if (head->side == 'L')
		head->side = 'R';
	else
		head->side = 'L';

	c_appendix = mvwinch (field, head->appendix_y, head->appendix_x);

	switch (Hit (field, 0, c_appendix)) {
		case -1:
			OneHead (field, head);
			return;
		case 2:
			tail = Grow (field, tail);
	}

	mvwaddch (field, head->neck->y, head->neck->x, head->neck->type);

	wattron (field, A_BOLD);

	mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);

	wattroff (field, A_BOLD);

	wrefresh (field);
}


/* Move the snake */
BODY *MoveSnake (WINDOW *field, HEAD *head, BODY *tail)
{
	BODY *aux;
	char c_head, c_appendix;	// what's ahead head and 2nd head

	aux = tail;
	while (aux->next != head->neck)
		aux = aux->next;

// after turning, keep the corner '/' or '\'
	if (turned == -1)
		head->neck->type = corner;

// change former neck [if not turning, it's a '=' or 'H']
	else if (head->mov == 'U' || head->mov == 'D') {
		if (!turned)
			head->neck->type = 'H';
	}
	else {
		if (!turned)
			head->neck->type = '=';
	}
	mvwaddch (field, head->neck->y, head->neck->x, head->neck->type);

// erase heads
	mvwaddch (field, tail->y, tail->x, ' ');
	mvwaddch (field, head->y, head->x, ' ');
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, ' ');

// we can't lose the BODY part hold by tail
	aux = tail;
// now the tail is former tail's next
	tail = tail->next;
// neck position is actual neck position, and will further be increased deppending on mov
	aux->x = head->neck->x;
	aux->y = head->neck->y;
// change necks xP
	head->neck->next = aux;
	aux->next = NULL;

	if (turned == 2)
		aux->type = corner;

	head->neck = aux;

// move!
	switch (head->mov) {
		case 'U':
// if just turned, neck follow head
			if (turned == 2)
				aux->x = head->x;
			else
				aux->y--;

			Boundary (&aux->y, &aux->x);

			head->y--;
			Boundary (&head->y, &head->x);

// if there's still the 2nd head, move it
			if (head->part[1]) {
				head->appendix_y--;
				Boundary (&head->appendix_y, &head->appendix_x);
// see neck type deppending on 2nd head's side
				if (turned != 2) {
					if (head->side == 'L')
						aux->type = '\\';
					else
						aux->type = '/';
				}
			}
// if there isn't, normal neck
			else if (turned != 2)
				aux->type = 'H';
			break;

		case 'D':
// if just turned, neck follow head
			if (turned == 2)
				aux->x = head->x;
			else
				aux->y++;

			Boundary (&aux->y, &aux->x);

			head->y++;
			Boundary (&head->y, &head->x);

// if there's still the 2nd head, move it
			if (head->part[1]) {
				head->appendix_y++;
				Boundary (&head->appendix_y, &head->appendix_x);
// see neck type deppending on 2nd head's side
				if (turned != 2) {
					if (head->side == 'L')
						aux->type = '\\';
					else
						aux->type = '/';
				}
			}
// if there isn't, normal neck
			else if (turned != 2)
				aux->type = 'H';
			break;

		case 'L':
// if just turned, neck follow head
			if (turned == 2)
				aux->y = head->y;
			else
				aux->x--;

			Boundary (&aux->y, &aux->x);

			head->x--;
			Boundary (&head->y, &head->x);

// if there's still the 2nd head, move it
			if (head->part[1]) {
				head->appendix_x--;
				Boundary (&head->appendix_y, &head->appendix_x);
// see neck type deppending on 2nd head's side
				if (turned != 2) {
					if (head->side == 'L')
						aux->type = '/';
					else
						aux->type = '\\';
				}
			}
// if there isn't, normal neck
			else if (turned != 2)
				aux->type = '=';
			break;

		case 'R':
// if just turned, neck follow head
			if (turned == 2)
				aux->y = head->y;
			else
				aux->x++;

			Boundary (&aux->y, &aux->x);

			head->x++;
			Boundary (&head->y, &head->x);

// if there's still the 2nd head, move it
			if (head->part[1]) {
				head->appendix_x++;
				Boundary (&head->appendix_y, &head->appendix_x);
// see neck type deppending on 2nd head's side
				if (turned != 2) {
					if (head->side == 'L')
						aux->type = '/';
					else
						aux->type = '\\';
				}
			}
// if there isn't, normal neck
			else if (turned != 2)
				aux->type = '=';
			break;
	}

// what's the new tail like? deppends on the next part
	if (tail->x < tail->next->x && tail->x == 1 && tail->next->x == WIDTH - 2)
		tail->type = '>';
	else if (tail->x < tail->next->x)
		tail->type = '<';

	else if (tail->x > tail->next->x && tail->x == WIDTH - 2 && tail->next->x == 1)
		tail->type = '<';
	else if (tail->x > tail->next->x)
		tail->type = '>';

	else if (tail->y < tail->next->y && tail->y == 1 && tail->next->y == HEIGHT - 2)
		tail->type = 'V';
	else if (tail->y < tail->next->y)
		tail->type = 'A';

	else if (tail->y > tail->next->y && tail->y == HEIGHT - 2 && tail->next->y == 1)
		tail->type = 'A';
	else if (tail->y > tail->next->y)
		tail->type = 'V';

// rewrite tail, neck and heads
	mvwaddch (field, tail->y, tail->x, tail->type);
	mvwaddch (field, aux->y, aux->x, aux->type);

	wattron (field, A_BOLD);

	c_head = mvwinch (field, head->y, head->x);

// if 2nd head: hit with it; else don't hit with it =P
	if (head->part[1])
		c_appendix = mvwinch (field, head->appendix_y, head->appendix_x);
	else
		c_appendix = 0;

	switch (Hit (field, c_head, c_appendix)) {
		case 1:
			Loser (field, head, tail);
			return tail;
		case -1:
			OneHead (field, head);
			break;
		case 2:
			tail = Grow (field, tail);
	}

	mvwaddch (field, head->y, head->x, head->part[0]);
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);

	wattroff (field, A_BOLD);

	wrefresh (field);

	return tail;
}


/* Turn 90° to somewhere */
void Turn (WINDOW *field, HEAD *head, BODY *tail, char turn_to)
{
	char c_head, c_appendix;

// erase head's
	mvwaddch (field, head->y, head->x, ' ');
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, ' ');

// it's going to...
	switch (turn_to) {
		case 'U':
// it was going where?
			if (head->mov == 'L')
				corner = '\\';
			else
				corner = '/';

			if (!turned)
				head->neck->type = '=';
// new type
			head->part[0] = 'V';
// if there is a 2nd head, it's new position [deppends on the side] and type
			if (head->part[1]) {
				head->appendix_y = head->y;
				if (head->mov == 'L') {
					head->appendix_x = head->x - 1;
					head->side = 'L';
				}
				else {
					head->appendix_x = head->x + 1;
					head->side = 'R';
				}
				Boundary (&head->appendix_y, &head->appendix_x);
				head->part[1] = 'V';
			}
			break;

		case 'D':
// it was going where?
			if (head->mov == 'L')
				corner = '/';
			else
				corner = '\\';

			if (!turned)
				head->neck->type = '=';
// new type
			head->part[0] = 'A';

// if there is a 2nd head, it's new position [deppends on the side] and type
			if (head->part[1]) {
				head->appendix_y = head->y;
				if (head->mov == 'L') {
					head->appendix_x = head->x - 1;
					head->side = 'R';
				}
				else {
					head->appendix_x = head->x + 1;
					head->side = 'L';
				}
				Boundary (&head->appendix_y, &head->appendix_x);
				head->part[1] = 'A';
			}
			break;

		case 'L':
// it was going where?
			if (head->mov == 'U')
				corner = '\\';
			else
				corner = '/';

			if (!turned)
				head->neck->type = 'H';
// new type
			head->part[0] = '>';

// if there is a 2nd head, it's new position [deppends on the side] and type
			if (head->part[1]) {
				head->appendix_x = head->x;
				if (head->mov == 'U') {
					head->appendix_y = head->y - 1;
					head->side = 'R';
				}
				else {
					head->appendix_y = head->y + 1;
					head->side = 'L';
				}
				Boundary (&head->appendix_y, &head->appendix_x);
				head->part[1] = '>';
			}
			break;

		case 'R':
// it was going where?
			if (head->mov == 'U')
				corner = '/';
			else
				corner = '\\';

			if (!turned)
				head->neck->type = 'H';
// new type
			head->part[0] = '<';

// if there is a 2nd head, it's new position [deppends on the side] and type
			if (head->part[1]) {
				head->appendix_x = head->x;
				if (head->mov == 'U') {
					head->appendix_y = head->y - 1;
					head->side = 'L';
				}
				else {
					head->appendix_y = head->y + 1;
					head->side = 'R';
				}
				Boundary (&head->appendix_y, &head->appendix_x);
				head->part[1] = '<';
			}
			break;
	}

	head->mov = turn_to;

	wattron (field, A_BOLD);

	c_head = mvwinch (field, head->y, head->x);
// if 2nd head: hit with it; else don't hit with it =P
	if (head->part[1])
		c_appendix = mvwinch (field, head->appendix_y, head->appendix_x);
	else
		c_appendix = 0;

	switch (Hit (field, c_head, c_appendix)) {
		case 1:
			Loser (field, head, tail);
			return;
		case -1:
			OneHead (field, head);
			break;
		case 2:
			tail = Grow (field, tail);
	}

	mvwaddch (field, head->y, head->x, head->part[0]);
	if (head->part[1])
		mvwaddch (field, head->appendix_y, head->appendix_x, head->part[1]);

	wattroff (field, A_BOLD);

	wrefresh (field);

	turned = 1;
}


/* Move the live stone! [actually it's a 'K'] */
void LiveStone (WINDOW *field, HEAD head)
{
	char c;	// only move if there's nothing on the way
	char y_dist, x_dist;	// distance from head, ortogonaly in each axis
	char anyway = 0;

	y_dist = head.y - live_y;	// + for below, - for above
	x_dist = head.x - live_x;	// + for right, - for left

	wattron (field, COLOR_PAIR (FGyellow) | A_BOLD);

	mvwaddch (field, live_y, live_x, ' ');
	wrefresh (field);

BEGIN:

// closer in the y axis
	if (abs (y_dist) < abs (x_dist) || anyway) {
		if (x_dist > 0 || anyway) {
			c = mvwinch (field, live_y, live_x + 1);
// nothing in the way, oyeah
			if (c == ' ' || c == ACS_VLINE || c == ACS_HLINE) {
				live_x++;
				goto END;
			}
		}
		else {
			c = mvwinch (field, live_y, live_x - 1);

			if (c == ' ' || c == ACS_VLINE || c == ACS_HLINE) {
				live_x--;
				goto END;
			}
		}
	}
// closer in the x axis
	if (y_dist > 0 || anyway) {
		c = mvwinch (field, live_y + 1, live_x);

		if (c == ' ' || c == ACS_VLINE || c == ACS_HLINE) {
			live_y++;
		}
	}
	else {
		c = mvwinch (field, live_y - 1, live_x);

		if (c == ' ' || c == ACS_VLINE || c == ACS_HLINE) {
			live_y--;
		}
	}

	if (y_dist == head.y - live_y && x_dist == head.x - live_x) {
		anyway = 1;
		goto BEGIN;
	}

END:
	Boundary (&live_y, &live_x);

	mvwaddch (field, live_y, live_x, 'K');

	wrefresh (field);

	wattrset (field, COLOR_PAIR (FGgreen));
}




int main ()
{
	WINDOW *field;
	BODY *tail;
	HEAD head;
	int c;	// for getch ()
	char frame = 0;	// for different frames needed for action

	srand (time (NULL));

	initscr ();
	keypad (stdscr, TRUE);
	start_color ();
	assume_default_colors (-1, -1);
	init_pair (BGhud, COLOR_WHITE, COLOR_GREEN);	// hud color
	init_pair (FGgreen, COLOR_GREEN, -1);	// green FG, for snake
	init_pair (FGred, COLOR_RED, -1);	// red FG, for food/apple/dying
	init_pair (FGyellow, COLOR_YELLOW, -1);	// yellow FG, for live stone
	init_pair (BGhelp, COLOR_WHITE, COLOR_BLUE);	// blue BG for help window
	init_pair (BGboom, COLOR_YELLOW, COLOR_RED);

	hud = subwin (stdscr, 1, COLS, 0, 0);
	wbkgd (hud, COLOR_PAIR (BGhud) | A_BOLD);
	mvwaddstr (hud, 0, COLS/2 - 4, "CURSNAKE");
	wrefresh (hud);

	mvaddstr (6, 0, "Dificulty [0~9 or 11] >");
	do {
		mvscanw (6, 24, "%d", &dificulty);
	} while (dificulty < 0 || dificulty == 10 || dificulty > 11);

	curs_set (0);
	move (6, 0);
	clrtoeol ();

	field = CreateField (dificulty);
	tail = CreateSnake (field, &head);
	ThrowFood (field);

	mvwaddstr (hud, 0, 0, "'?': Help");
	ReHud ();

	noecho ();
	getch ();
	nodelay (stdscr, TRUE);

// main loop
	while (c != 'q') {
		if (turned == 1)
			turned = 2;	// can't turn, but move

		if (c = getch ()) {
// flush input buffer: for no tilt on holding a button; only if got something in getch ()
			flushinp ();
		}

		switch (c) {
// pause: Space to unpause, '?' for help, 'q' to quit
			case ' ':
				attron (A_BOLD);
				nodelay (stdscr, FALSE);
				mvaddstr (FIELD_y0 + HEIGHT, COLS/2 - 2, "PAUSE");
				do {
					c = getch ();
				} while (c != ' ' && c != '?' && c != 'q');
				nodelay (stdscr, TRUE);
				mvaddstr (FIELD_y0 + HEIGHT, COLS/2 - 2, "     ");
				refresh ();
				if (c != '?')
					break;
// help
			case '?':
				Help ();
				break;

// change 2nd head's side
			case 'z':
				if (head.part[1] && !turned)
					Switch (field, &head, tail);
				break;

// turn if possible [not going to same direction and not turned the previous turn]
			case KEY_UP: case 'w':
				if (head.mov != 'U' && head.mov != 'D' && turned <= 0)
					Turn (field, &head, tail, 'U');
				break;

			case KEY_DOWN: case 's':
				if (head.mov != 'U' && head.mov != 'D' && turned <= 0)
					Turn (field, &head, tail, 'D');
				break;

			case KEY_LEFT: case 'a':
				if (head.mov != 'L' && head.mov != 'R' && turned <= 0)
					Turn (field, &head, tail, 'L');
				break;

			case KEY_RIGHT: case 'd':
				if (head.mov != 'L' && head.mov != 'R' && turned <= 0)
					Turn (field, &head, tail, 'R');
				break;
		}

		if (turned != 1 && dificulty != -1) {
			tail = MoveSnake (field, &head, tail);
			if (turned == 2)
				turned = -1;
			else
				turned = 0;
		}

		if (dificulty == -1)
			break;


		if (frame % (5 * ((dificulty + 5)/5)) == 0) {
			LiveStone (field, head);
			frame = 0;
		}


		if (head.mov == 'U' || head.mov == 'D')
			usleep (18e4 - (dificulty * 15e3));
		else
			usleep (17e4 - (dificulty * 15e3));

		frame++;
	}

	endwin ();
	return 0;
}
