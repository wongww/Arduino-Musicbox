/********************************************
 *
 *Name: William Wong
 *Email: wongww@usc.edu
 *Section: Wed 12:30 PM
 *Assignment: Project - Music Box
 *
 ********************************************/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include "lcd.h"
#include "adc.h"
#define NUM_NOTES 21
#define NUM_TONES 26

unsigned char *selectScreen(int page);
void displayScreen(int page);
void getNumNotes();
void divideNotes();
void buzzerOff();
void buzzerOn();
void setOCR1A(int frequency);
void timerInit();
void timerOn();
void timerOff();
int numCycles(int frequency);
void playNotes();
void playNote(int frequency);
void timer2Init();
void timer2On();
void timer2Off();
void updateNote(int newNote);
void saveCopy();
int getNoteAtCursor();
int checkMem(unsigned char *memory);
//Position of Cursor
volatile unsigned char positionR = 0;
volatile int positionC = 0;
//Current Page the 
volatile int currentPage = 1;
//Counter for the number of times to turn on and off the buzzer
volatile int counter = 0;
//The frequency to lay the song at
volatile int frequencyToPlay = 0;
volatile int playingNote = 1;
//Used for Rotary Encode
volatile unsigned char a, b, new_state, old_state;
//Detect is rotary encoder was changed
volatile unsigned char changed = 0;
//Count used in ISR 
volatile int count = 0;
//Index of current note being played 0-20
volatile int indexOfCurrentNote = 1;
//Coordinates of Cursor
//Used for versions of C before c99 in for loops
volatile int i = 0;
volatile int y = 0;
//Booleans to check if there is a page on left or right
volatile int isPageLeft = 0;
volatile int isPageRight = 0;
//3 Separate Screens, each with a pointer
unsigned char screen1[7] = { 0, 0, 0, 0, 0, 0, 0 };
unsigned char *ptr1 = screen1;
unsigned char screen2[7] = { 0, 0, 0, 0, 0, 0, 0 };
unsigned char *ptr2 = screen2;
unsigned char screen3[7] = { 0, 0, 0, 0, 0, 0, 0 };
unsigned char *ptr3 = screen3;
/*
  The note_freq array contains the frequencies of the notes from C3 to C5 in
  array locations 1 to 25.  Location 0 is a zero for rest(?) note.  Used
  to calculate the timer delays.
*/
unsigned int note_freq[NUM_TONES] = { 0, 131, 139, 147, 156, 165, 176, 185, 196, 208, 220, 233, 247,
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523
};

//List of all notes
char *noteLetters[26] = { "  ", "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ",
  "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ", "C " };
//Number of notes including rests the song has
int numNotes = 21;
/*Some sample tunes for testing */
/*
  The "notes" array is used to store the music that will be played.  Each value
  is an index into the tone value in the note_freq array.
*/

// G, A, F, F(octive lower), C     Close Encounters
//unsigned char notes[NUM_NOTES] = {20, 22, 18, 6, 13};

// D D B C A B G F# E D A C A B    Fight On
//unsigned char notes[NUM_NOTES] = {15, 15, 12, 13, 10, 12, 8, 7, 5, 3, 10, 13, 10, 12};

// E E F G G F E D C C D E E D D   Ode to Joy
unsigned char notes[NUM_NOTES] = { 17, 17, 18, 20, 20, 18, 17, 15, 13, 13, 15, 17, 17, 15, 15 };

//Copy of notes
unsigned char *copy;
unsigned char mem[21];
//QUESTIONS:

//3 screens with 7 notes each
int main(void)
{
  // Initialize various modules and functions
  adc_init();
  lcd_init();
  timerInit();

  //Enable PORTB bit3 and bit4 as output
  DDRB |= 0b00011000;
  //Enable PORTC bit1 and bit5 as inputs
  DDRC &= ~(0b00100010);
  //Enable Pull Up Resistors
  PORTC |= 0b00100010;
  //Set Interrupts
  PCICR |= 1 << PCIE1;
  PCMSK1 |= ((1 << PCINT13) | (1 << PCINT9));
  //Enable interrupts
  sei();

  //Initiallize Rotary Encoder state
  unsigned char init_read = PINC;
  a = init_read &(1 << PC1);
  b = init_read &(1 << PC5);
  if (!b && !a)
    old_state = 0;
  else if (!b && a)
    old_state = 1;
  else if (b && !a)
    old_state = 2;
  else
    old_state = 3;

  new_state = old_state;

  // Show splash screen for 2 seconds
  lcd_writecommand(1);
  lcd_stringout("William Wong");
  _delay_ms(2000);
  lcd_writecommand(1);
  eeprom_read_block(mem, (void*) 0, 21);

  //Check For Reset. No Press is 255 ADC Value
  unsigned char adc_result = adc_sample(0);
  if (!(adc_sample(0) >= 0xFD && adc_sample(0) <= 0xFF))
  {
    _delay_ms(100);
    while (!(adc_sample(0) >= 0xFD && adc_sample(0) <= 0xFF))
    {
      //Debounce
    }
    _delay_ms(100);
    //Load the standard song into a working copy
    copy = notes;
  }
  //If memory has a valid song, load that song instead
  else if (checkMem(mem))
  {
    copy = mem;
  }
  //If there is nothing in memory and reset was not pressed, load default song
  else
  {
    copy = notes;
  }

  //Initial display of the first screen
  divideNotes();
  displayScreen(1);
  //Main loop of program
  while (1)
  {
    //Detect if note is being changed and update it
    if (changed)
    {
      changed = 0;
      updateNote(count);
    }
    //If select is pressed, begin playing song
    //Select is 204-208
    adc_result = adc_sample(0);
    if (adc_result >= 0xCC && adc_result <= 0xD0)
    {
      _delay_ms(100);
      while (adc_sample(0) >= 0x64 && adc_sample(0) <= 0x68)
      {
        //do nothing
      }
      _delay_ms(100);
      //Begin playing notes
      playNotes();
      //Write to memory
      eeprom_update_block(copy, (void*) 0, 21);
    }
    //If Up/Right is pressed UP:49-54 Right:0-5
    else if ((adc_result >= 0x31 && adc_result <= 0x36) || (adc_result >= 0 && adc_result <= 0x05))
    {
      _delay_ms(200);
      while ((adc_sample(0) >= 0x31 && adc_sample(0) <= 0x36) || (adc_sample(0) >= 0 && adc_sample(0) <= 0x05))
      {
        //do nothing until button is lifted
      }
      _delay_ms(200);
      if (positionC == 13 && isPageRight)
      {
        displayScreen(currentPage + 1);
        currentPage++;
      }
      else if (positionC < 13)
      {
        positionC = positionC + 2;
        lcd_moveto(positionR, positionC);
      }
    }
    //If Down/Left is pressed Down: 100-104 Left:152-157
    else if ((adc_result >= 0x64 && adc_result <= 0x68) || (adc_result >= 0x98 && adc_result <= 0x9D))
    {
      _delay_ms(200);
      while ((adc_sample(0) >= 0x64 && adc_sample(0) <= 0x68) || (adc_sample(0) >= 0x98 && adc_sample(0) <= 0x9D))
      {
        //do nothing until button is lifted
      }
      _delay_ms(200);
      if (positionC == 1 && isPageLeft)
      {
        displayScreen(currentPage - 1);
        lcd_moveto(0, 13);
        positionC = 13;
      }
      else if (positionC > 1)
      {
        positionC = positionC - 2;
        lcd_moveto(positionR, positionC);
      }
    }
  }
  return 1;
}

/*
  Code for initializing TIMER1 and its ISR
*/
ISR(PCINT1_vect)
{
  // Read the input bits and determine A and B
  unsigned char input = PINC;
  a = input &(1 << PC1);
  b = input &(1 << PC5);
  //Get the note number at cursor
  count = getNoteAtCursor();
  if (old_state == 0)
  {
    if (a)
    {
      new_state = 1;
      if (count < 25)
      {
        count++;
      }
    }
    else if (b)
    {
      new_state = 3;
      if (count > 0)
      {
        count--;
      }
    }
  }
  else if (old_state == 1)
  {
    if (b)
    {
      new_state = 2;
      if (count < 25)
      {
        count++;
      }
    }
    else if (!a)
    {
      new_state = 0;
      if (count > 0)
      {
        count--;
      }
    }
  }
  else if (old_state == 2)
  {
    if (!a)
    {
      new_state = 3;
      if (count < 25)
      {
        count++;
      }
    }
    else if (!b)
    {
      new_state = 1;
      if (count > 0)
      {
        count--;
      }
    }
  }
  else
  {
    if (!b)
    {
      new_state = 0;
      if (count < 25)
      {
        count++;
      }
    }
    else if (a)
    {
      new_state = 2;
      if (count > 0)
      {
        count--;
      }
    }
  }
  if (new_state != old_state)
  {
    changed = 1;
    old_state = new_state;
  }
}

ISR(TIMER1_COMPA_vect)
{
  //Interrupt is occuring every half period of a given frequency. 
  if (counter <= frequencyToPlay)
  {
    if (counter % 2 == 0)
    {
      PORTB |= 0b00010000;
    }
    else
    {
      PORTB &= ~(0b00010000);
    }
    counter++;
  }
}
//PMW Clock
ISR(TIMER2_COMPA_vect)
{
  OCR2A = ((indexOfCurrentNote) *255) / numNotes;
}

unsigned char *selectScreen(int page)
{
  if (page == 1)
  {
    return ptr1;
  }
  else if (page == 2)
  {
    return ptr2;
  }
  else if (page == 3)
  {
    return ptr3;
  }
  else
  {
    return ptr3;
  }
}
//Display the screen the page is on
void displayScreen(int page)
{
  //Clear page
  lcd_writecommand(1);
  //Get notes for that this 
  unsigned char *tempScreen = selectScreen(page);
  //Page 2 or 3
  if (page > 1)
  {
    lcd_moveto(0, 0);
    lcd_stringout("<");
    lcd_moveto(1, 0);
    char buff[2];
    snprintf(buff, 2, "%d", page - 1);
    lcd_stringout(buff);
    isPageLeft = 1;
  }
  //Page 1
  else if (page == 1)
  {
    isPageLeft = 0;
    //Is there a Page2?
    if (numNotes > 7)
    {
      isPageRight = 1;
      lcd_moveto(0, 15);
      lcd_stringout(">");
      lcd_moveto(1, 15);
      lcd_stringout("2");
    }
  }
  //Is there a Page 3
  if (page == 2 && numNotes > 14)
  {
    lcd_moveto(0, 15);
    lcd_stringout(">");
    lcd_moveto(1, 15);
    lcd_stringout("3");
  }
  //Begin writing notes
  int position = 1;
  for (i = 0; i < 7; i++)
  {
    int noteNumber = tempScreen[i];
    lcd_moveto(0, position);
    lcd_stringout(noteLetters[noteNumber]);
    //Calculate Octave and Print it out
    if (noteNumber != 25 && noteNumber!=0)
    {
      lcd_moveto(1, position);
      char oct[2];
      snprintf(oct, 2, "%d", noteNumber / 13 + 3);
      lcd_stringout(oct);
    }
    //High C is Octave 5 and print it out
    else if (noteNumber == 25)
    {
      lcd_moveto(1, position);
      char oct[2];
      snprintf(oct, 2, "%d", noteNumber / 13 + 4);
      lcd_stringout(oct);
    }
    position += 2;
  }
  //Print Page number for Current Page in Bottom Left
  lcd_moveto(1, 0);
  char pg[2];
  snprintf(pg, 2, "%d", page);
  lcd_stringout(pg);
  //Return cursor to 0,1
  lcd_moveto(0, 1);
  //Update cursor coordinates
  positionR = 0;
  positionC = 1;
}
//Get the number of notes the song has
void getNumNotes()
{
  int tempNum = 21;
  for (i = 20; i >= 0; i--)
  {
    if (copy[i] == '0')
    {
      tempNum--;
    }
    else
    {
      numNotes = tempNum;
      return;
    }
  }
}
//Divide the 21 or less notes
void divideNotes()
{
  //Get number of notes in song
  getNumNotes();
  unsigned char *temps[3];
  for (i = 0; i < numNotes; i++)
  {
    temps[i / 7][i % 7] = copy[i];
  }
  ptr3 = temps[2];
  ptr2 = temps[1];
  ptr1 = temps[0];
  return;
}

//Turn Off Buzzer
void buzzerOff()
{
  //PB4
  PORTB &= ~(0b00010000);
}
//Turn On Buzzer
void buzzerOn()
{
  PORTB |= 0b00010000;
}
//Get and OCR1A which represent half a period
void setOCR1A(int frequency)
{
  OCR1A = 31250 / frequency;
}
//Configurations for Initializing the Buzzer
void timerInit()
{
  //Set CTC mode
  TCCR1B |= (1 << WGM12);
  //Enable Timer Interupt
  TIMSK1 |= (1 << OCIE1A);
}
//Configurations for PWM LED 
void timer2Init()
{
  //Set PWM mode
  TCCR2A |= (1 << WGM21) | (1 << WGM20);
  //non inverting mode
  TCCR2A |= (1 << COM2A1);
  //Enable Timer Interupt
  TIMSK2 |= (1 << OCIE2A);
  OCR2A = ((indexOfCurrentNote) *255) / numNotes;
}

//Start counting up to OCR1A, the threshold
void timerOn()
{
  //Start timer and set prescaler of 256
  TCCR1B |= (1 << CS12);
}
//Start counting PWM; turn on timer
void timer2On()
{
  // Start PWM
  TCCR2B |= (1 << CS21);
}
//Stop timer2
void timer2Off()
{
  //Stop PWM
  TCCR2B &= ~(1 << CS21);
  OCR2A = 0;
  timer2On();
}
//Stop counting up to setOCR1A
void timerOff()
{
  //Turn Off timer
  TCCR1B &= ~(1 << CS12);
}
//Play all the notes
void playNotes()
{
  //Determine how many notes will be played
  getNumNotes();
  timer2Init();
  timer2On();
  for (i = 0; i < numNotes; i++)
  {
    int note_index = copy[i];
    playNote(note_freq[note_index]);
    indexOfCurrentNote++;
  }
  timer2Off();
}
//Play a single note given it's frequency for ~0.5 seconds.
//Do not use any Delays, unless it's for a REST
void playNote(int frequency)
{
  //If Rest delay 0.5 sec and continue
  if (!frequency)
  {
    _delay_ms(500);
    return;
  }
  frequencyToPlay = frequency;
  setOCR1A(frequency);
  timerOn();
  //Wait until the note is finish
  while (counter <= frequency)
  {
    //do nothing
  }
  //Reset Counter
  counter = 0;
  timerOff();
}
//Get the note the cursor is hovering over
int getNoteAtCursor()
{
  int index = positionC / 2;
  return copy[index + (currentPage - 1) *7];
}
//Update the note the cursor is on
void updateNote(int newNote)
{
  int index = positionC / 2;
  copy[index + (currentPage - 1) *7] = newNote;
  lcd_stringout(noteLetters[newNote]);
  //Not a rest note:
  if (newNote)
  {
    lcd_moveto(1, positionC);
    char oct[2];
    if (newNote != 25)
    {
      snprintf(oct, 2, "%d", newNote / 13 + 3);
      lcd_stringout(oct);
    }
    else
    {
      lcd_stringout("5");
    }
    //moveBackUp
  }
  //If the note is a rest, then do this:
  else if (newNote == 0)
  {
    lcd_moveto(1, positionC);
    lcd_stringout(" ");
  }
  lcd_moveto(0, positionC);
}
//Check if memory is valid and contains a song to load
int checkMem(unsigned char *memory)
{
  for (i = 0; i < NUM_NOTES; i++)
  {
    if (memory[i] == 0xFF || memory[i] > 0x15)
    {
      return 0;
    }
  }
  return 1;
}