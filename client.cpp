
#include <Arduino.h>
#include "Client.h"
#include "Display.h"   // Update lcd display

typedef enum {
              Position_Check,
              Wait_For_ACK,
              Wait_Ship_Int,
              Send_Response,
              Move_Cursor,
              End} State;
char* stateNames[] = {
              "Position_Check",
              "Wait_For_ACK",
              "Wait_Ship_Int",
              "Send_Response",
              "Move_Cursor",
              "End"
              };

/*
 * helper functions for main and sub client
 * used to convert data type, send data, wait for response, check received character
 */
// write char to serial3
void uint8_to_serial3(uint32_t num) {
  Serial3.write((char) (num >> 0));
}

// read from serial3, convert char to integer
uint8_t uint8_from_serial3() {
  uint8_t num = 0;
  num = num | ((uint8_t) Serial3.read()) << 0;
  return num;
}

// wait to receive n bytes of data
bool wait_on_serial3( uint8_t nbytes, long timeout ) {
  unsigned long deadline = millis() + timeout;
  while (Serial3.available()<nbytes && (timeout<0 || millis()<deadline))
  {
    delay(1);
  }
  return Serial3.available()>=nbytes;
}

// check the returning value(0 -> MISS, 1 -> HIT)
bool checkResp(uint8_t response) {
  if(response == 0){
    return false;
  }else{
    return true;
  }
}

// indicating whether screens are set up on arduino two
bool receive_CR() {
	if(wait_on_serial3(1, 1000) && Serial3.read() == 'C') {
		return true;
	}else {
		return false;
	}
}

// 'OK', you may continue to next state
bool receive_ACK() {
  if(wait_on_serial3(2, 1000) && (Serial3.read() == 'K')) {
    return true;
  }else {
    return false;
  }
}

// send pressed coordinate to server
uint8_t sendPosition() {
  // convertion from pixel position to grid index
  uint8_t ret = (g_cursorX+2)/12*10 + (g_cursorY-30)/12;
  return ret;
}

/*
 * sub function for client
 * useful for a) Send current position to arduino two
 *            b) Based on the response from server, update display on screen
 */
bool clientSub(uint8_t pos) {
  bool occupied;
  State client = Position_Check;

  while(client != End) {
    if(client == Position_Check) {
      Serial3.write('A');    // send ack to server
      uint8_to_serial3(pos); // send position
      client = Wait_For_ACK;
    }
    else if((client == Wait_For_ACK) && receive_ACK()) {
      uint8_t response = uint8_from_serial3();
      occupied = checkResp(response); //
      Serial3.write('E'); // send end message
      client = End;
      checkGrid();
      break;
    }
    else {
      client = Position_Check;
    }
  }
  return occupied;
}

/*
 * main function for client
 * useful for a) Wait for ship initialization on arduino two
 *            b) Allow cursor movement on arduino one
 *            c) Determination of game end
 */
void clientMain() {
  // initial state: wait for ship initialization
  State current = Wait_Ship_Int;
  while (true) {
    select = digitalRead(SEL);
    if(current == Wait_Ship_Int && receive_CR()){ // setup complete on arduino two
      current = Move_Cursor;
    }
    else if(current == Move_Cursor) { // move cursor to take the guess
      cursorMovement();
      if(select == LOW){ // position is selected
        uint8_t curr_pos = sendPosition();   // send current position to arduino two
        bool occupied = clientSub(curr_pos); // check if this position is occupied by ships from arduino two
        updateGridPos(occupied);             // use the result to update grid on arduino one
        checkGrid();
        if (hit_counter == 17) { // all the ships are hit, end of game
            Winner();
            break;
          }
        switchTurn();            // switch turn between two players
        fontUpdate();            // update font display
        delay(150);
      }
    }
    else{
      current = Wait_Ship_Int;
    }
  }
}