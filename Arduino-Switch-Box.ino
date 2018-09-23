/*

    Arduino-Switch-Box.ino

    Implementation of a 4-Bulb Magic Switch Box
    
    Copyright (C) 2018  olikraus@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 
*/

/*
#define BULB0 9
#define BULB1 6
#define BULB2 5
#define BULB3 10

#define SWITCH0 8
#define SWITCH1 7
#define SWITCH2 4
#define SWITCH3 3
*/

#define BULB0 5
#define BULB1 6
#define BULB2 9
#define BULB3 10

#define SWITCH0 A0
#define SWITCH1 A1
#define SWITCH2 A2
#define SWITCH3 A3


#define ANALOG_WRITE_BULB 70

uint8_t light_to_pin[4] = { BULB0, BULB1, BULB2, BULB3 };
uint8_t switch_to_pin[4] = { SWITCH0, SWITCH1, SWITCH2, SWITCH3 };



#define SWITCH_ON 1
#define TIMEOUT 3000

uint8_t is_locked;            // indicates, whether the switch board is locked. If so, only the unlock sequence is allowed

uint8_t switch_status[4];     // debounced & valid position of the switches, can be read by all parts of the program
#define DEBOUNCE_CNT 3
uint8_t switch_debounce[4];   // internal array for the debounce algorithm
uint8_t is_switch_changed;    // set to 1 if any switch has changed, reset to 0 in the main loop
uint8_t last_switch_turned_off; // the number (0..3) of the switch, which was turned off
uint8_t last_switch_on_cnt;  // number of "on" switches before the last change
uint8_t switch_on_cnt;    // current number of "on" switches
uint8_t switch_sop;     // switch position, represented as sum of products 10=Hi, 01=Low

uint8_t max_on_cnt;     // last number max number of "on" switches

// arguments for max_on_cnt:
// max_on_cnt==1: max_on_cnt_pos1 --> on switch
// max_on_cnt==2: max_on_cnt_pos1/2 --> on switch
// max_on_cnt==3: max_on_cnt_pos1 --> off switch
// max_on_cnt==4: max_on_cnt_pos1 --> last switch which was turned off
int8_t max_on_cnt_pos1 = -1;
int8_t max_on_cnt_pos2 = -1;


// master switch matrix
// for each switch, return the light bulb
// for most tricks, only map_switch_to_light[switch][0] is used, additionally
// the ASCII graphics only works with map_switch_to_light[switch][0]
// one switch may activate multiple bulbs, in this case the second dimension contains all the bulbs
// bulbs are always filled starting from position 0
// if map_switch_to_light[switch][0] is negative, then this switch does not drive any bulb
int8_t map_switch_to_light[4][4];    



// state variable and definitions for the execute_action() procedure
#define ACTION_STATE_INACTIVE 0
#define ACTION_STATE_USER_SWAP_WAIT 1
#define ACTION_STATE_ASSIGN0_WAIT 2
#define ACTION_STATE_ASSIGN1_WAIT 3
#define ACTION_STATE_ASSIGN2_WAIT 4

uint8_t action_state = ACTION_STATE_INACTIVE;

int8_t max_on_cnt_npos1 = -1;
int8_t max_on_cnt_npos2 = -1;
int8_t user_pos1;
int8_t user_pos2;


uint8_t action_assign1234_bitmask; // used by the assign 1,2,3,4 action


//==============================================
// switch position debounce and detection
//==============================================


void init_switch(void)
{
  uint8_t i;
  for( i = 0; i < 4; i++ )
  {
    pinMode(switch_to_pin[i], INPUT_PULLUP);    
    switch_debounce[i] = 0;
    switch_status[i] = 2;
    map_switch_to_light[i][0] = i;
    map_switch_to_light[i][1] = -1;
    map_switch_to_light[i][2] = -1;
    map_switch_to_light[i][3] = -1;
  }
}

void read_switch_status(void)
{
  uint8_t i, v, on_cnt;
  int8_t pos1, pos2, npos1;
  for( i = 0; i < 4; i++ )
  {
     v = digitalRead(switch_to_pin[i]);
     if ( switch_debounce[i] > 0 )
     {
      if ( switch_status[i] == v )
      {
        switch_debounce[i] = 0; // do nothing, wait for other values
      }
      else
      {
        switch_debounce[i]--;
        if ( switch_debounce[i] == 0 )
        {
          switch_status[i] = v; // new value detected
          is_switch_changed = 1;
          if ( v != SWITCH_ON )
            last_switch_turned_off = i;
          break;    // break out of the loop, so that we detect only one change at a time
        }
      }
     }
     else if ( switch_status[i] != v )
     {
        switch_debounce[i] = DEBOUNCE_CNT;  // start debounce
     }
  }
  
  on_cnt = 0;
  pos1 = -1;
  pos2 = -1;
  switch_sop = 0;
  for( i = 0; i < 4; i++ )
  {
    if ( switch_status[i] == SWITCH_ON )
    {
      on_cnt++;
      if ( pos1 < 0 )
        pos1 = i;
      else  
        pos2 = i;

      switch_sop<<=2;
      switch_sop|=2;    // High      
    }
    else
    {
      npos1 = i;
      switch_sop<<=2;
      switch_sop|=1;    // Low
    }
  }
  
  if ( switch_on_cnt != on_cnt )
  {
    last_switch_on_cnt = switch_on_cnt;
    switch_on_cnt = on_cnt; 
    if ( last_switch_on_cnt < switch_on_cnt )
    {
      max_on_cnt = on_cnt;
      if ( max_on_cnt == 1 )
      {
        max_on_cnt_pos1 = pos1; 
        max_on_cnt_pos2 = -1; 
      }
      else if ( max_on_cnt == 2 )
      {
        max_on_cnt_pos1 = pos1; 
        max_on_cnt_pos2 = pos2; 
      }
      else if ( max_on_cnt == 3 )
      {
        max_on_cnt_pos1 = npos1; 
        max_on_cnt_pos2 = -1; 
      }
      else if ( max_on_cnt == 4 )
      {
        max_on_cnt_pos1 = -1; 
        max_on_cnt_pos2 = -1; 
      }
      else
      {
        max_on_cnt_pos1 = -1; 
        max_on_cnt_pos2 = -1; 
      }
    }
    else if ( last_switch_on_cnt > switch_on_cnt )
    {
      if ( max_on_cnt == 4 )
      {
        if ( switch_on_cnt == 1 )
        {
          max_on_cnt_pos1 = pos1; 
          max_on_cnt_pos2 = -1;     
        }
      }
    } 
  }  
}

void print_switch_status(void)
{
  uint8_t i;
  
  Serial.print(last_switch_on_cnt);
  Serial.print(">");
  Serial.print(switch_on_cnt);
  Serial.print("/");
  Serial.print(max_on_cnt);
  Serial.print("\n");
  
  Serial.print(max_on_cnt_pos1);
  Serial.print("/");
  Serial.print(max_on_cnt_pos2);
  Serial.print("\n");
  
  for( i = 0; i < 4; i++ )
    Serial.print(switch_status[i] == SWITCH_ON ? "*" : ".");
  Serial.print("\n");
}


//==============================================
// show the current switch to bulb mapping 
// generate ASCII graphics for this
// output with Serial.print
//==============================================

char map_matrix[6][16];   // this matrix will contain the ASCII graphics for the mapping

void clear_mapping()
{
  uint8_t i, j;
  for( i = 0; i < 6; i++ )
    for( j = 0; j < 16; j++ )
      map_matrix[i][j] = ' ';
}

void draw_mapping_line(uint8_t i, uint8_t j, char c)
{
  if ( map_matrix[i][j] != 32 )
  {
    map_matrix[i][j] = '+';
  }
  else
  {
    map_matrix[i][j] = c;
    
  }
}

void print_mapping(void)
{
  uint8_t i, j;
  
  // clear the ASCII buffer
  clear_mapping();

  // build the ASCII graphics
  for( i = 0; i < 4; i++ )
  {
    if ( map_switch_to_light[i][0] < 0 )
    {
      // do nothing
    }
    else if ( i == map_switch_to_light[i][0] )
    {
      for(j = 0; j < 6; j++ )
        draw_mapping_line(j, map_switch_to_light[i][0]*4+1, '|');
        //map_matrix[j][map_switch_to_light[i][0]*4+1] = '|';
    }
    else if ( i < map_switch_to_light[i][0] )
    {
      for(j = i+2; j < 6; j++ )
        draw_mapping_line(j, i*4+1,'|');
      map_matrix[i+1][i*4+1] = '/';
      for( j = i*4+2; j < map_switch_to_light[i][0]*4; j++ )
        draw_mapping_line(i+1, j, '-');
      map_matrix[i+1][map_switch_to_light[i][0]*4] = '/';
      for(j = 0; j < i+1; j++ )
        draw_mapping_line(j, map_switch_to_light[i][0]*4, '|');
    }
    else
    {
      for(j = i+2; j < 6; j++ )
        draw_mapping_line(j, i*4+1,'|');
      map_matrix[i+1][i*4+1] = '\\';
      for( j = map_switch_to_light[i][0]*4+2; j < i*4+1; j++ )
        draw_mapping_line(i+1, j,'-');
      map_matrix[i+1][map_switch_to_light[i][0]*4+2] = '\\';
      for(j = 0; j < i+1; j++ )
        draw_mapping_line(j, map_switch_to_light[i][0]*4+2, '|');
    }
  } 

  // output the ASCII graphics
  Serial.print(": ");
  for( i = 0; i < 4; i++ )
  {
    Serial.print("L");
    Serial.print(i);
    Serial.print("  ");
  }
  Serial.println("");
  for( j = 0; j < 6; j++ )
  {
    Serial.print(": ");
    for( i = 0; i < 16; i++ )
    {
      Serial.print(map_matrix[j][i]);
    }
    Serial.println("");
  }
  Serial.print(": ");
  for( i = 0; i < 4; i++ )
  {
    Serial.print("S");
    Serial.print(i);
    if ( switch_status[i] == SWITCH_ON )
      Serial.print("* ");
    else
      Serial.print("  ");    
  }
  Serial.println("");
    
}

//==============================================
// output the light
//==============================================


void init_light(void)
{
  uint8_t i;
  for( i = 0; i < 4; i++ )
  {
    pinMode(light_to_pin[i], OUTPUT);    
    analogWrite(light_to_pin[i], 0);
  }
}

void write_switch_to_light(void)
{
  uint8_t i, j;
  uint8_t bulb[4];
  for( i = 0; i < 4; i++ )
    bulb[i] = 0;
  
  /*
  for( i = 0; i < 4; i++ )
  {
    if ( map_switch_to_light[i][0] >= 0 )
    {
      if ( switch_status[i] == SWITCH_ON )
      {
        analogWrite(light_to_pin[map_switch_to_light[i][0]], ANALOG_WRITE_BULB);
      }
      else
        analogWrite(light_to_pin[map_switch_to_light[i][0]], 0);
    }
  }
  */

  for( i = 0; i < 4; i++ )
  {
    if ( switch_status[i] == SWITCH_ON )
    {
      for( j = 0; j < 4; j++ )
        if ( map_switch_to_light[i][j] >= 0 )
          bulb[map_switch_to_light[i][j]] = 1;
    }
  }
  /*
  for( i = 0; i < 4; i++ )
  {
    Serial.print(bulb[i])
    Serial.print(" ");
  }
  Serial.println("");
  */

  for( i = 0; i < 4; i++ )
    if ( bulb[i] != 0 )
      analogWrite(light_to_pin[i], ANALOG_WRITE_BULB);
    else
      analogWrite(light_to_pin[i], 0);

    

  
}



//==============================================
// action procedures and map manipulation
//==============================================

// do nothing
void action_null(void)
{
}

// reset the mapping to its default values
void action_reset(void)
{
  uint8_t i;
  for( i = 0; i < 4; i++ )
  {
    map_switch_to_light[i][0] = i;
    map_switch_to_light[i][1] = -1;
    map_switch_to_light[i][2] = -1;
    map_switch_to_light[i][3] = -1;
  }
  Serial.println(F("Die Schalter sind wieder mit den Lampen gegenüber verbunden."));
}

void action_exchange_two_bulbs(void)
{
    action_state = ACTION_STATE_USER_SWAP_WAIT;
    init_sequence();
    
    user_pos1 = -1;
    user_pos2 = -1;    
    Serial.println(F("Der Zuschauer darf zwei Lampen vertauschen. Danach zuerst die Lampen anschalten, die NICHT vertauscht wurden."));  
}

void action_assign_all_bulbs(void)
{
  uint8_t i;
  action_state = ACTION_STATE_ASSIGN0_WAIT;
  action_assign1234_bitmask = 0;
  for( i = 0; i < 4; i++ )
  {
    map_switch_to_light[i][0] = -1;
    map_switch_to_light[i][1] = -1;
    map_switch_to_light[i][2] = -1;
    map_switch_to_light[i][3] = -1;
  }
  
  Serial.println(F("Der Zuschauer darf ALLE Lampen vertauschen. Danach die Lampen von links nach rechts aktivieren."));
}

void action_pile(void)
{
  uint8_t i;
  //uint8_t t = (last_switch_turned_off + 1) & 3;
  uint8_t t = last_switch_turned_off;
  for( i = 0; i < 4; i++ )
  {
    map_switch_to_light[i][0] = -1;
    map_switch_to_light[i][1] = -1;
    map_switch_to_light[i][2] = -1;
    map_switch_to_light[i][3] = -1;
  }
  for( i = 0; i < 4; i++ )
    map_switch_to_light[t][i] = i;
  Serial.print(F("Schalter "));
  Serial.print(t);
  Serial.println(F(" aktiviert nun alle Lampen."));
}

void action_lock(void)
{
  action_reset();
  is_locked = 1;
  Serial.println(F("Das Switch Board wurde gesperrt. Entsperren nur durch die Unlock-Sequenz."));
}

void action_unlock(void)
{
  is_locked = 0;
  Serial.print(F("Das Switch Board wurde entsperrt.\n"));
}

void execute_action(void)
{
  
  switch(action_state)
  {
    case ACTION_STATE_INACTIVE:
      break;
    case ACTION_STATE_USER_SWAP_WAIT: // user choice 
      {
        uint8_t i, j;
        for( i = 0; i < 4; i++ )
        {
          if ( user_pos1 != i && switch_status[i] == SWITCH_ON )
          {
            if ( user_pos1 < 0 )
              user_pos1 = i;
            else
            {
              uint8_t tmp;
              
              user_pos2 = i;
              tmp = 1<<user_pos1;
              tmp |= 1<<user_pos2;
              max_on_cnt_npos1 = -1;
              max_on_cnt_npos2 = -1;
              
              for( j = 0; j < 4; j++ )
              {
                if ( ( tmp & (1<<j) ) == 0 )
                {
                  if ( max_on_cnt_npos1 < 0 )
                    max_on_cnt_npos1 = j;
                  else
                    max_on_cnt_npos2 = j;
                }
              }
              
              
              tmp = map_switch_to_light[max_on_cnt_npos1][0];
              map_switch_to_light[max_on_cnt_npos1][0] = map_switch_to_light[max_on_cnt_npos2][0];
              map_switch_to_light[max_on_cnt_npos2][0] = tmp;
              
              Serial.print(F("swap2\n"));
              Serial.print(max_on_cnt_npos1);
              Serial.print(F("<>"));
              Serial.print(max_on_cnt_npos2);
              Serial.print(F("\n"));
              
              action_state = ACTION_STATE_INACTIVE;
            }
          }
        }
      
      }
      break;
    case ACTION_STATE_ASSIGN0_WAIT:
      {
        uint8_t i;
        for( i = 0; i < 4; i++ )
          if ( map_switch_to_light[i][0] < 0 )
            if ( switch_status[i] == SWITCH_ON )
            {
              map_switch_to_light[i][0] = 0;
              action_assign1234_bitmask |= 1<<i;
              action_state = ACTION_STATE_ASSIGN1_WAIT;
            }
      }
      break;
    case ACTION_STATE_ASSIGN1_WAIT:
      {
        uint8_t i;
        for( i = 0; i < 4; i++ )
          if ( map_switch_to_light[i][0] < 0 )
            if ( switch_status[i] == SWITCH_ON )
            {
              map_switch_to_light[i][0] = 1;
              action_assign1234_bitmask |= 1<<i;
              action_state = ACTION_STATE_ASSIGN2_WAIT;
            }
      }
      break;
    case ACTION_STATE_ASSIGN2_WAIT:
      {
        uint8_t i;
        for( i = 0; i < 4; i++ )
          if ( map_switch_to_light[i][0] < 0 )
            if ( switch_status[i] == SWITCH_ON )
            {
             
              map_switch_to_light[i][0] = 2;
              action_assign1234_bitmask |= 1<<i;
              for( i = 0; i < 4; i++ )
                if ( (action_assign1234_bitmask & (1<<i)) == 0 )
                  map_switch_to_light[i][0] = 3;
              action_state = ACTION_STATE_INACTIVE;
              break;
            }
      }
      break;
    default:
      action_state = ACTION_STATE_INACTIVE;
      break;
  }
  
}

//==============================================
// switch sequence detection
//==============================================

#define S0_HI (2<<6)
#define S1_HI (2<<4)
#define S2_HI (2<<2)
#define S3_HI (2<<0)

#define S0_LO (1<<6)
#define S1_LO (1<<4)
#define S2_LO (1<<2)
#define S3_LO (1<<0)

#define S0_DC (3<<6)
#define S1_DC (3<<4)
#define S2_DC (3<<2)
#define S3_DC (3<<0)

struct sequence
{
  char *name;
  uint8_t seq[7];
  uint8_t cnt;
  void (*action)();
  uint8_t dly; // 100ms
  uint8_t is_lock_state_sequence;
  uint8_t pos;
  uint32_t wait_until;  // system time in milliseconds
};

struct sequence seq_list[] = 
{
  
  // reset sw0 on & off
  {"reset",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 3, action_reset, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

#define SIMPLE_CMDS
#ifdef SIMPLE_CMDS
  // reassign all bulbs: sw1 on & off
  {"all",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_HI|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 3, action_assign_all_bulbs, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // pile all markers  2:on, 2:off
  {"pile",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 3, action_pile, /*dly=*/ 30, /*lock=*/ 0, 0, 0},


  // pile all markers  2:on, 0:on, 2:off, 0:off
  {"pile",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_HI|S1_LO|S2_HI|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_pile, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // pile all markers  2:on, 1:on, 2:off, 1:off
  {"pile",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_HI|S2_HI|S3_LO, 
     S0_LO|S1_HI|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_pile, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // pile all markers  2:on, 3:on, 2:off, 3:off
  {"pile",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_HI, 
     S0_LO|S1_LO|S2_LO|S3_HI, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_pile, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

#else

  // pile all markers  2:on, 1:on, 2:off, 0&3:on, 2:on, then: all off. (last off + 1) Mod 4 --> pile
  {"pile",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_HI|S2_HI|S3_LO,
     S0_DC|S1_HI|S2_LO|S3_DC,
     S0_HI|S1_HI|S2_HI|S3_HI, 
     S0_DC|S1_DC|S2_DC|S3_DC, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 7, action_pile, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // exchange two bulbs: sw1 on & off
  {"x",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_HI|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_exchange_two_bulbs, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // reassign all bulbs: sw1 on & off
  {"all",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_HI|S2_LO|S3_LO, 
     S0_LO|S1_HI|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_assign_all_bulbs, /*dly=*/ 30, /*lock=*/ 0, 0, 0},
#endif

  // lock sequence: sw0 on, sw3 on, sw0 off, sw3 off
  {"lock",{ 
     S0_LO|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_LO|S3_LO, 
     S0_HI|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_LO|S2_HI|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_lock, /*dly=*/ 30, /*lock=*/ 0, 0, 0},

  // unlock sequence: first three on, sw1 (middle switch) last off
  {"unlock",{ 
     S0_DC|S1_DC|S2_DC|S3_LO, 
     S0_HI|S1_HI|S2_HI|S3_LO,
     S0_DC|S1_HI|S2_DC|S3_LO, 
     S0_LO|S1_HI|S2_LO|S3_LO, 
     S0_LO|S1_LO|S2_LO|S3_LO}, /*cnt=*/ 5, action_unlock, /*dly=*/ 1, /*lock=*/ 1, 0, 0}

};

void init_sequence(void)
{
  uint8_t i;

  is_locked = 0;  // ensure, that the switch board is unlocked
  
  for ( i = 0; i < sizeof(seq_list)/sizeof(struct sequence); i++ )
  {
    seq_list[i].pos = 0;
  }
}

void show_all_sequence_status(void)
{
  uint8_t i;
  for ( i = 0; i < sizeof(seq_list)/sizeof(struct sequence); i++ )
  {
    if ( seq_list[i].is_lock_state_sequence == is_locked )
    {
      Serial.print(F("Sequence '"));
      Serial.print(seq_list[i].name);
      Serial.print(F("': pos="));
      Serial.print(seq_list[i].pos);
      Serial.print(F(" cnt="));
      Serial.print(seq_list[i].cnt);
      Serial.println("");
    }
  }
}



// pos == 0..cnt-1    match until pos found
// pos == cnt         check for and setup delay
// pos == cnt+1       wait until delay is over
// pos == cnt+2       execute action
void check_sequence(uint8_t i)
{
  struct sequence *seq = seq_list+i;

  if ( action_state != ACTION_STATE_INACTIVE )
  {   
    // do not check any sequence as long as any action is required
    // instead, always reset the sequence
    seq->pos = 0; 
    return;  
  }

  
   
  
  if ( seq->pos == 0 )
  {
    // initial case, check whether the sequence can be started
    // seq->seq may contain don't cares, a match is there, if switch_sop AND seq_list[i].seq[0] is not changed compared to switch_sop
    if ( (switch_sop & seq->seq[0]) == switch_sop )
    {
      seq->pos++;
    }
  }
  else if ( seq->pos < seq->cnt )
  {
    // check, whether the next pattern is matched
    if ( (switch_sop & seq->seq[seq->pos]) == switch_sop )
    {
      seq->pos++;
    }
    // check whether the current pattern still matches
    else if ( (switch_sop & seq->seq[seq->pos-1]) != switch_sop )
    {
      // if not, then reset the sequence recognition 
      seq->pos = 0;
    }
  }
  // no else
  if ( seq->pos == seq->cnt )
  {
    // the sequence is fully detected
    // is a there delay required?
    if ( seq->dly > 0 )
    {
      uint8_t j;
      // reset all other sequences; just to ensure, that they do not distub each other and are activated in parallel
      for ( j = 0; j < sizeof(seq_list)/sizeof(struct sequence); j++ )
      {
        if ( j != i )
          seq_list[j].pos = 0;
      }

      // calclate target time until we need to wait
      seq->wait_until = millis() + seq->dly*100;
      seq->pos++;
    }
    else
    {
      seq->pos = seq->cnt + 2;
    }
  }
  // no else
  if ( seq->pos == seq->cnt+1 )
  {
    // the last pattern must always be valid during during wait
    if ( (switch_sop & seq->seq[seq->cnt-1]) != switch_sop )
    {
      // if not, then reset the sequence recognition 
      seq->pos = 0;
    }
    else if ( seq->wait_until < millis() )
    {
      // delay is over, the action can be executed
      seq->pos = seq->cnt + 2;
    }
  }
  // no else
  if ( seq->pos == seq->cnt+2 )
  {
    Serial.print(F("Sequence '"));
    Serial.print(seq->name);
    Serial.println(F("' activated."));
    seq->action();
    seq->pos = 0;
  }
}

void check_all_sequence(void)
{
  uint8_t i;

  // with all sequences
  for ( i = 0; i < sizeof(seq_list)/sizeof(struct sequence); i++ )
  {
    // check only sequences, which are valid for the current lock state
    if ( seq_list[i].is_lock_state_sequence == is_locked )
    {
      check_sequence(i);
    }
  }
}


//==============================================
// Arduino setup
//==============================================

void setup() 
{
  // as quick as possible: ensure that all lights are off
  pinMode(BULB0, OUTPUT);
  analogWrite(BULB0, 0);
  pinMode(BULB1, OUTPUT);
  analogWrite(BULB1, 0);
  pinMode(BULB2, OUTPUT);
  analogWrite(BULB2, 0);
  pinMode(BULB3, OUTPUT);
  analogWrite(BULB3, 0);

  init_switch();
  init_light();
  init_sequence();
  
  Serial.begin(9600);
}

//==============================================
// Arduino main loop
//==============================================

void loop() 
{
  // read the status of the four switches into global variables
  read_switch_status();

  // check for any command sequence
  check_all_sequence();

  // some commands will cause extra actions, this is handled here
  execute_action();

  // if the switch status was changed, enlight the bulbs accordingly
  if ( is_switch_changed != 0 )
  {
    // print_switch_status();
    print_mapping();
    write_switch_to_light();
    show_all_sequence_status();
    is_switch_changed = 0;
  }

}

