#pragma once

const int I2C_SDA = 21;
const int I2C_SCL = 22; //for the MU6050 sensor 

const int leftMtr_A = 16;  //TX2, The motor controller uses a pwm signal to control speed
const int leftMtr_B = 17;  //RX2
const int rightMtr_A = 18; //D18
const int rightMtr_B = 19; //D19

const int KP = 40;
const int KI = 40;
const int KD = 0.05;