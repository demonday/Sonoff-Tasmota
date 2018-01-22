/*
  xdrv_i2c_dimmer.ino - Sonoff Controller for i2c Dimmer

  Uses 10Hz Timer to Monitor:
      Settings.power
      Settings.light_state
      Settings.light_fade
    and send a an appropiate brightness signal to an i2c slave to
    control the dim level.

  Operates as an i2c Master and sends a corresponding byte from 0-100
  representing the brightness level (%) via I2C to the slave.

  It requests  the current brightness from the i2c slave to validate
  and ensure the required level is achieved.

  Copyright (C) 2018  Niall Gormley

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

#ifdef USE_I2C_DIMMER

#define I2C_SLAVE_ADDRESS 0x4    // Address of the Dimmer
#define D_LOG_DIMMER "I2CDIM: "  // Log Prefix

#define FADE_STEP 3

volatile uint8_t cur_bri = 0;    // Current Brightness of I2C dimmer
volatile uint8_t target_bri = 0; // Brightness level to get the dimmer to
volatile uint8_t checkCounter=0;     // Slow requests to i2c slave
volatile bool reset = false;

void DimmerInit()
{
  //AddLog_P(LOG_LEVEL_DEBUG, PSTR(D_LOG_DIMMER "DimmerInit"));

  // Send  a RESET and WAIT
  pinMode(pin[GPIO_RESET],OUTPUT);
  digitalWrite(pin[GPIO_RESET],LOW);
  delay(50);
  digitalWrite(pin[GPIO_RESET],HIGH);
  delay(50);

  Wire.begin();
  sendLevel(0); // off
  dimmer_start_timer(); // start the timer
}

// Monitor the state and dim level and dispatch
// the appropiate i2c instruction to the dimmer
ICACHE_RAM_ATTR void dimmer_timer_isr()
{
  checkCounter++;
  if(Settings.power == 0)
  {
    target_bri = 0;
  }
  else
  {
    target_bri = Settings.light_dimmer;
  }

  // If we aren't at the required level send a signal to get us there
  if(cur_bri != target_bri)
  {
    uint8_t req_bri=0;
    if(Settings.light_fade == 1 && abs(cur_bri-target_bri)>FADE_STEP && !reset)
    {
      if(cur_bri > target_bri)
      {
        req_bri = cur_bri-FADE_STEP;
      }
      else
      {
        req_bri = cur_bri+FADE_STEP;
      }
    }
    else
    {
      req_bri=target_bri;
      reset=false;
    }

    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_DIMMER "Change %d"), req_bri);
    AddLog(LOG_LEVEL_INFO);
    sendLevel(req_bri);
    cur_bri = req_bri;
  }

  // Check every second to see what the i2c slave reports
  // as its achieved level
  if(checkCounter==20) // Once per second
  {
      int read = Wire.requestFrom(I2C_SLAVE_ADDRESS, 1);
      uint8_t level = Wire.read();
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_DIMMER "Brightness %d:%d"), level, cur_bri);
      AddLog(LOG_LEVEL_DEBUG);
      if(level==255) //I2C Failed.... reset it....
      {
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_DIMMER "Dim Reset"));
          AddLog(LOG_LEVEL_INFO);
          digitalWrite(pin[GPIO_RESET],LOW);
          delay(5);
          digitalWrite(pin[GPIO_RESET],HIGH);
          delay(5);
          reset=true;
          sendLevel(cur_bri);
      }
      else if(level != cur_bri)
      {
        cur_bri = level;
      }
      checkCounter=0;
  }
}


void dimmer_start_timer()
{
    timer1_attachInterrupt(dimmer_timer_isr);
    timer1_isr_init();
    timer1_enable(TIM_DIV265, TIM_EDGE, TIM_LOOP); //312.5Khz
    timer1_write(15625); // 20 Hz
}

void sendLevel(uint8_t level)
{
  Wire.beginTransmission(I2C_SLAVE_ADDRESS);
  Wire.write(level);      // sends brightness level
  Wire.endTransmission();
}

#endif //USE_I2C_DIMMER
