Koden: 
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>

SdFat sd;
SFEMP3Shield MP3player;
state_m playing_state;

int counter = 0;//Counter for flashing lights

const int getBag = A2;  //Yellow Led 
const int eatNuts = A3; //Yellow led
const int brushTeeth = A4;  //Yellow Led
const int getDressed = A1;  //Yellow Led 
const int getBooks = A0;  //Yellow Led 
const int labyrinthTrack = A5;  //input from points
const int collectedAllItems = 5;  //Green Led
const int hitWall = 10; //Red Led 
const int thingsToPickUp [] = { //Array of all the yellow lights
  eatNuts,
  brushTeeth,
  getDressed,
  getBooks,
  getBag,
 };

void setup() {

  pinMode(getBooks, OUTPUT);  //A0
  pinMode(getDressed, OUTPUT);  //A1
  pinMode(getBag, OUTPUT);  //A2
  pinMode(eatNuts, OUTPUT); //A3
  pinMode(brushTeeth, OUTPUT);  //A4
  pinMode(labyrinthTrack, INPUT_PULLUP);  //A5
  pinMode(collectedAllItems, OUTPUT); //5
  pinMode(hitWall, OUTPUT); //10

  setLEDSLow();//Set all the LEDs low initially

  uint8_t result; //result code from some function as to be tested at later time.
  Serial.begin(115200);

  //Initialize the SdCard.
  if (!sd.begin(SD_SEL, SPI_FULL_SPEED))
    sd.initErrorHalt();
  if (!sd.chdir("/"))
    sd.errorHalt("sd.chdir");

  //Initialize the MP3 Player Shield
  result = MP3player.begin(); //Method from the MP3 library
  Serial.print(result);
  Serial.println("reset");
}

void loop() {
//  checkAnalogRangeOfPoints(); //Method for calibrating the touch points of the labyrinth
 playing_state = ready;
 digitalWrite(10, LOW);

//Check to see if anything has been touched, determined by the method getLabyrinthPoint();
  if (getLabyrinthPoint() == 1) { //Walls of the track
    playWallTrack();
    Serial.println("playWallTrack");
  } else if (getLabyrinthPoint() == 2) { //eatNuts 
    playEatNutsTrack();
    Serial.println("playEatNutsTrack");
  } else if (getLabyrinthPoint() == 3) { //brushTeeth
    playBrushTeethTrack();
    Serial.println("playBrushTeethTrack");
  } else if (getLabyrinthPoint() == 4) { //getDressed 
    playGetDressedTrack();
    Serial.println("playGetDressedTrack");
  } else if (getLabyrinthPoint() == 5) { //getBooks
    playGetBooksTrack();
    Serial.println("playGetBooksTrack");
  } else if (getLabyrinthPoint() == 6) { //getBag
    playGetBagTrack();
    Serial.println("playGetBagTrack");
   }

   //If user is touching the goal but haven’t collected all items 
  if (getLabyrinthPoint() == 7 && !checkIfDone()){ 	//notDone 
    playNotDoneTrack();
    Serial.println("playNotDoneTrack");
  }

  if (checkIfDone()) { 	//If all items collected:
    digitalWrite(collectedAllItems, HIGH);
    if (getLabyrinthPoint() == 7) {
      MP3player.stopTrack(); //Stop the currently playing track, method from the MP3 library.
      delay(100);
      playMP3(7);
      Serial.println("Starting the reset procedure");
      delay(3000); //Wait 3 seconds
      resetLabyrinth();
    }
  }
}

//Method used for calibrating points, not used in the normal run of the program
void checkAnalogRangeOfPoints() {
  Serial.print("Value: ");
  Serial.println(analogRead(labyrinthTrack));
  delay(1000);
}

//Method that resets the labyrinth
void resetLabyrinth() {
  flashLEDS();
  setLEDSLow();
  counter = 0;
  Serial.println("The labyrinth has been reset!");
  delay(100);
}

//Method for flashing the leds
void flashLEDS() {
  while (counter < 9) {
    if (digitalRead(eatNuts) == LOW) {
      setLEDSHigh();
      delay(250);
    } else {
        setLEDSLow();
        delay(250);
    }
    counter++;
  }
}

//Sets all the LEDs low. Used for making all the LEDs blink and resetting the labyrinth.
void setLEDSLow() {
  digitalWrite(eatNuts, LOW);
  digitalWrite(brushTeeth, LOW);
  digitalWrite(getDressed, LOW);
  digitalWrite(getBooks, LOW);
  digitalWrite(getBag, LOW);
  digitalWrite(hitWall, LOW);
  digitalWrite(collectedAllItems, LOW);
}

//Sets all the LEDS high. Used for making all the LEDs blink.
void setLEDSHigh() {
  digitalWrite(eatNuts, HIGH);
  digitalWrite(brushTeeth, HIGH);
  digitalWrite(getDressed, HIGH);
  digitalWrite(getBooks, HIGH);
  digitalWrite(getBag, HIGH);
  digitalWrite(hitWall, HIGH);
  digitalWrite(collectedAllItems, HIGH);
}

//PLays the track assosiated with touching the wall, and sets the red led high, then low after a short period of time.
void playWallTrack() {
  digitalWrite(hitWall, HIGH);
  playMP3(1);
  delay(50);
  digitalWrite(hitWall, LOW);
}

//Play the track assosiated with touching the goal point, but without having all items collected.
void playNotDoneTrack() {
  playMP3(8);
}

//Play the track assosiated with touching the brush teeth point, sets the led to high
void playBrushTeethTrack() {
  digitalWrite(A4, HIGH);
  playMP3(3);
}

//Play the track assosiated with touching the eat nuts point, sets the led to high
void playEatNutsTrack() {
  digitalWrite(A3, HIGH);
  playMP3(2);
}

//Play the track assosiated with touching the get dressed point, sets the led to high
void playGetDressedTrack() {
  digitalWrite(A1, HIGH);
  playMP3(4);
}

//Play the track assosiated with touching the get books point, sets the led to high
void playGetBooksTrack() {
  digitalWrite(A0, HIGH);
  playMP3(5);
}

//Play the track assosiated with touching the bag point, sets led to high
void playGetBagTrack() {
  digitalWrite(A2, HIGH);
  playMP3(6);
}

//Method for determinging which point that was touched.
int getLabyrinthPoint() {
//  int analogReadNumber = analogRead((labyrinthTrack));
  //checks if the range is within any of the defined intervals, if one of them is true it returns the number of that point.
  //If none of the points were touched but we got a reading below 1020 (since it reads 1023 without any input) it defaults to the track walls instead.
  if (analogRead(labyrinthTrack) >= 983 && analogRead(labyrinthTrack) <= 990) {//eatNuts 983-990
    delay(100);
    if (analogRead(labyrinthTrack) >= 983 && analogRead(labyrinthTrack) <= 990 ) {
       Serial.print("Value p2:");
       Serial.println(analogRead(labyrinthTrack));
       return 2;
    }
  } else if (analogRead(labyrinthTrack) >= 44 && analogRead(labyrinthTrack) <= 46) {//brushTeeth 44-46
      delay(100);
      if (analogRead(labyrinthTrack) >= 44 && analogRead(labyrinthTrack) <= 46) {
        Serial.print("Value p3:");
        Serial.println(analogRead(labyrinthTrack));
        return 3;
      }
  } else if (analogRead(labyrinthTrack) >= 30 && analogRead(labyrinthTrack) <= 32) {//getDressed 30-32
      delay(100);
      if (analogRead(labyrinthTrack) >= 30 && analogRead(labyrinthTrack) <= 32) {
        Serial.print("Value p4:");
        Serial.println(analogRead(labyrinthTrack));
        return 4;
      }
  } else if (analogRead(labyrinthTrack) >= 226 && analogRead(labyrinthTrack) <= 230) {//getBooks 226 - 230
      delay(100);
      if (analogRead(labyrinthTrack) >= 226 && analogRead(labyrinthTrack) <= 230) {
        Serial.print("Value p5:");
        Serial.println(analogRead(labyrinthTrack));
        return 5;
      }
  } else if (analogRead(labyrinthTrack) >= 122 && analogRead(labyrinthTrack) <= 131) {//getBag 122 - 131
      delay(100);
      if (analogRead(labyrinthTrack) >= 122 && analogRead(labyrinthTrack) <= 131) {
        Serial.print("Value p6:");
        Serial.println(analogRead(labyrinthTrack));
        return 6;
      }
  } else if (analogRead(labyrinthTrack) >= 20 && analogRead(labyrinthTrack) <= 23 ) {//Done 20-23
      delay(100);
      if (analogRead(labyrinthTrack) >= 20 && analogRead(labyrinthTrack) <= 23) {
        Serial.print("Value p7:");
        Serial.println(analogRead(labyrinthTrack));
        return 7;
      }
    } else if (analogRead(labyrinthTrack) < 1020) {
      delay(10);
      if (analogRead(labyrinthTrack) < 1020) {
      Serial.println("Valuep1");
      Serial.println(analogRead(labyrinthTrack));
        return 1;
      }
    }
}


//Checks if all the items have been collected by reading the state of the led pins. If all of them are high, return true, else return false
boolean checkIfDone() {
  for (int i = 0; i < 5; i++) {
    const int pinToCheck = thingsToPickUp[i];
    if (digitalRead(pinToCheck) == LOW)
      return false;
  }
  return true;
}

//Plays the track number it gets passed by the method call.
void playMP3(int track) {
  if (MP3player.isPlaying() != 1) {
    playing_state = playback;
    uint8_t result = MP3player.playTrack(track);
  }
}

//Reads the playstate of the mp3-player and returns an int depending on which state it is in.
uint8_t isPlaying() {
  uint8_t result;
  if(!digitalRead(MP3_RESET))
   result = 3;
  else if(getState() == playback)
   result = 1;
  else if(getState() == paused_playback)
   result = 1;
  else
   result = 0;

  return result;
}

//Returns the current play_state of the mp3-shield.
state_m getState() {
 return playing_state;
}

