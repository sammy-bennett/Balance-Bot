#pragma once

const int I2C_SDA = 21; //I2C DATA D21
const int I2C_SCL = 22; //I2C clock D22

const int leftMtr_A = 16;  //TX2, The motor controller uses a pwm signal to control speed
const int leftMtr_B = 17;  //RX2
const int rightMtr_A = 18; //D18
const int rightMtr_B = 19; //D19

const int KP = 10; //this motor is acually a little underpowered
const int KI = 1.5;
const int KD = 1;