/*
 * 
  Darth Fader - Code for running a USB-Powered, Mackie Control Protocol Generating Device
  Developed and maintained by Steven Strong since 2020.
  Additional information is available ... somewhere... I have no idea where this project is going to live yet.
  Released under whatever license makes it so people can use it for their projects if they think it's helpful, and if they sell it and get rich, they can send me a christmas card and some cookies or whatever.
 
*/

#include "Adafruit_NeoTrellis.h" 
#include "ClickEncoder.h"
#include <TimerOne.h>
#include <MIDIUSB.h>
#include <ShiftRegister74HC595.h>


// CONFIGURATION / PREFERENCES

int highestAudioValue = 12;
int currentAudioValue = 0;
int audioHoldCountdown = 0;
int audioHoldAtValue = 30;
int audioDecay = 4;
int audioDecayCountdown = 0;
bool buttonHeld = false;


// DEFINE BUTTON COLORS
// TODO: There should be multiple color maps, based on whatever mode we're in.

uint32_t buttonColorsDefault[] = { 
   0x7e7e00 , 0x0080FF, 0x000000, 0x7e0000, 
   0x7e007e , 0x7e007e, 0x7e007e, 0x007e00, 
   0x0080FF , 0x7e0000, 0x0080FF, 0x000000, 
   0x00007e , 0x00007e, 0x00007e, 0x00007e 
};

// DEFINE DEFAULT BUTTON BEHAVIORS - These are MIDI values that conform to the Mackie Control Protocol
// TODO : Need to document all of the defaults for different application. This is going to make my life 100 times easier
//    when trying to set up mappings.  First will be premiere, then I'll consider other apps as needed.
// Of note: These midi values map to functions that I was able to define in the button mapping in premiere.  Just putting this device together, and plugging it in
//    will only get you a handful of working functions (play/save/etc), to make this fully work as it's currently programmed, you'll need to set up button mapping
//    for the device in premiere as well.  I will set up a table that shows that mapping.

int buttonBehaviorsDefault[] = {
   55 , 57 , 0 , 80 ,
   70 , 71 , 72 , 94 ,
   88 , 89 , 90 , 0 ,
   58 , 59 , 60 , 61 
};

// ****************************************************
// TRELLIS INITIALIZATION 
// if you don't have a Trellis, then you need to replace this whole section with whatever you're using for buttons.
// ****************************************************

  Adafruit_NeoTrellis trellis;

  //define a callback for key presses
  TrellisCallback blink(keyEvent evt){

    // Check is the pad pressed?
    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
      trellis.pixels.setPixelColor(evt.bit.NUM, Wheel(map(evt.bit.NUM, 0, trellis.pixels.numPixels(), 0, 255))); //on rising
      Serial.print("Button Pressed: ");
      Serial.println(evt.bit.NUM);
      Serial.println(buttonBehaviorsDefault[evt.bit.NUM]);
      if (evt.bit.NUM == 99){
        buttonBehaviorsDefault[9] = buttonBehaviorsDefault[9]+1;
              Serial.print("New button 9 Value: ");
          Serial.println(buttonBehaviorsDefault[9]);
      } else {
        sendNote(buttonBehaviorsDefault[evt.bit.NUM]);
      }
    } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
    // or is the pad released?
      trellis.pixels.setPixelColor(evt.bit.NUM, buttonColorsDefault[evt.bit.NUM]); //off falling
    }
  
    // Turn on/off the neopixels!
    trellis.pixels.show();
    return 0;
  }
// END TRELLIS  ******************************************

// ****************************************************
// ROTARY ENCODER INITIALIZATION
// This whole project revolves (PUN!) around the use of a rotary encoder, but you could theoretically
// replace that with three buttons (up/down/click) if that's what you have.
// ****************************************************

  ClickEncoder *encoder;
  
  int16_t last, value;
  void timerIsr() {
    encoder->service();
  }
// END ENCODER  ******************************************


// ****************************************************
// SHIFT REGISTER INITIALIZATION
// Another totally optional function - The 74HC595 is used here to visualize audio output from Premiere.
// ****************************************************

ShiftRegister74HC595<1> sr(4, 6, 5);

// END SHIFTREG ****************************************************

void setup() {
  
  Serial.begin(9600);


  // DO A LITTLE BIT OF ANIMATION TO VERIFY THAT LEDS ARE WORKING
  // If your LEDS light out of order, this will tell you that you've wired up your shift reg. incorrectly.
  
  for (int i = 0; i < 8; i++) {
    sr.set(i, HIGH); // set single pin HIGH
    delay(50); 
    sr.setAllLow();
  }


  // TURN ON THE ENCODER *************************
  
  encoder = new ClickEncoder(A1, A0, A2, 4);  // I'm using pins A0,1 and 2 for the encoder.
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 
  last = -1;

  // TURN ON THE TRELLIS *************************
  
    if (!trellis.begin()) {
      Serial.println("Could not start trellis, check wiring?");
      while(1);
    } else {
      Serial.println("NeoPixel Trellis started");
    }

  // ACTIVATE ALL BUTTONS AND SET CALLBACKS
    for(int i=0; i<NEO_TRELLIS_NUM_KEYS; i++){
      trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
      trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
      trellis.registerCallback(i, blink);
    }

  // DO A LITTLE ANIMATION TO SHOW THAT THE TRELLIS IS UP AND RUNNING
    for (uint16_t i=0; i<trellis.pixels.numPixels(); i++) {
      trellis.pixels.setPixelColor(i, Wheel(map(i, 0, trellis.pixels.numPixels(), 0, 255)));
      trellis.pixels.show();
      delay(50);
    }
    for (uint16_t i=0; i<trellis.pixels.numPixels(); i++) {
      trellis.pixels.setPixelColor(i, buttonColorsDefault[i]);
      trellis.pixels.show();
      delay(50);
    }
  
}

void loop() {

  // FIRST, HANDLE THE DISPLAY OF THE AUDIO METER ***********************

  if (currentAudioValue > 0) {
 
    // we're showing lights;
    for (int vol = 0; vol < 8; vol++){
        // set the LEDS to match the currentAudioValue.
        sr.setNoUpdate(vol, LOW);
    }
    Serial.println(currentAudioValue);
    for (int vol = 0; vol < currentAudioValue-1; vol++){
        // set the LEDS to match the currentAudioValue.
        sr.setNoUpdate(vol, HIGH);
    }
    sr.updateRegisters(); 

   // Serial.println(audioHoldCountdown);

    // first, run the holdAt timer 
    if (audioHoldCountdown < audioHoldAtValue) {
      // increment the holdCountdown by one until we meet the timer
      audioHoldCountdown++;
    } else {
      // we've met the timer - reset the hold countdown, and then we can start decaying
       if (audioDecayCountdown == audioDecay){
       // if the tick reaches the value set for audioDecay, decrease the currentAudioValue by one, until it hits zero. 
        currentAudioValue -= 1;
        // reset the audioDecayCountdown
        audioDecayCountdown = 0;
      
        }
        // increase the tick on the audio decay timer audioDecayCountdown
       audioDecayCountdown++;
    } 
  } else {
      // currentValue is back to zero. we can re-set the audioHoldCountdown
      audioHoldCountdown = 0;
  }

  // HANDLE ANY IMPORT FROM THE MIDI CHANNEL?     ***********************
  
    midiEventPacket_t rx;
    rx = MidiUSB.read();
      
    /* DEBUG STUFF: This is useful for reverse engineering applications that send data over midi back to the device.
       if (rx.header != 0) {
       Serial.print(rx.header);
        Serial.print("-");
        Serial.print(rx.byte1);
        Serial.print("-");
        Serial.print(rx.byte2);
        Serial.print("-");
        Serial.println(rx.byte3);
        
      }
    */

    // THIS IS THE MEAT OF THE AUDIO VISUALIZER - I'm really just trusting that Premiere is sending that audio with the 
    // rx header = 13. This seems 100% true, and I have no reason to doubt it.  What is NOT true, is the range of values that
    // are sent under this header at byte 2.  It's somewhere between a max of 12, and a max of 50?  I have no idea. Need to figure this out.
    
    if (rx.header != 0 && rx.header == 13) {
      
      // 11 = something about movement
      // 13 = something about audio levels
      // if it's 13, there's two numbers... something low and something high.
      
        int curVol = int(rx.byte2);
        // get percentage of maxVol;
        float percentVol = float(curVol) / float(highestAudioValue);
        float numLEDS = percentVol * 8; // number of LEDs on my volume visualizer
        
        //Serial.print(ceil(numLEDS));
        
        currentAudioValue = numLEDS;
        // should set hold
         audioHoldCountdown = 0;
  
    }

  // NEXT, HANDLE ANY INTERACTION WITH THE TRELLIS ***********************
  // If the user has pressed a button, the functions are handled elsewhere.
    trellis.read();  

  // FINALLY, HANDLE ANY INTERACTION WITH THE ROTARY ENCODER *************
  // I considered making it so you could hold down the dial, then rotate to have something happen, but it was clunky and unreliable. There might 
  //    still be some code in here for that. I'll eventually remove that.
  // Of note: Double clicking changes the behavior of the scrub.  I found in Premiere that if you scrub using their default "jogging" behavior, that
  //    you can't hear the audio while you scrub. I have no idea why they did this, and that's a big thing for me, so I made it so you can double click
  //    and it will change to a behavior where you're sending a midi command to step through frames instead, which DOES play the audio.
  
  
  value += encoder->getValue(); 
  if (value != last) {
    if (value > last) {
      if (buttonHeld){
        Serial.println("Zoom In");
      } else {
        if (encoder->getAccelerationEnabled()){
          sendNote(54);
        } else {
          programChange(0,60,1);
        }
      }
    }
    if (value < last) {
      if (buttonHeld){
        Serial.println("Zoom OUT");
      }
      else { 
        if (encoder->getAccelerationEnabled()){
          sendNote(56);
        } else {
        Serial.println("Turning Down");
        programChange(0,60,65);
        }
      }
    }
    last = value;
  }
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    #define VERBOSECASE(label) case label: Serial.println(#label); break;
    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);

      case ClickEncoder::Held:
          //Serial.println("ClickEncoder::Held");
          buttonHeld = true;
          
        break;
      case ClickEncoder::Released:
          //Serial.println("ClickEncoder::Released");
          buttonHeld = false;
          
        break;
      case ClickEncoder::Clicked:
          Serial.println("ClickEncoder::Clicked");
          // Not sure what pushing the button should do right now... maybe select the current track?
          sendNote(73);
          
        break;
      case ClickEncoder::DoubleClicked:
          Serial.println("ClickEncoder::DoubleClicked");
          encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
          Serial.print("  Acceleration is ");
          Serial.println((encoder->getAccelerationEnabled()) ? "enabled" : "disabled");
        break;
    }
  }
  
  // AT THE END OF EACH LOOP, MAKE SURE TO SHUT DOWN THE MIDI COMMUNICATION. YOU DON'T WANT TO KEEP NOTES OPEN.
  noteOff(0, 94, 0);
  MidiUSB.flush();
  
  delay(20); //the trellis has a resolution of around 60hz
}


// UTILITY FUNCTIONS ******************************************/

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return trellis.pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return trellis.pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return trellis.pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  return 0;
}


void sendNote(byte pitch) {
  midiEventPacket_t note = {0x09, 0x90 | 0, pitch, 127};
  MidiUSB.sendMIDI(note);
  MidiUSB.flush();
  note = {0x09, 0x90 | 0, pitch, 0};
  MidiUSB.sendMIDI(note);
  MidiUSB.flush();
}

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void programChange(byte channel, byte program, byte val) {
  midiEventPacket_t pc = {0x0B, 0xB0, program, val};
  MidiUSB.sendMIDI(pc);
}
