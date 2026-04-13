#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

#include <pin_definitions.h> // pins and the PID constants 

hw_timer_t *My_timer = NULL; //for timer.

float accangle, gyroangle, gyrorate, curangle, prevangle(0), error, preverror, errorsum; 
const float targ_angle(-2.5),sampletime(0.01); //sample time in seconds, also used for the interupt!! 

int motorpwr(0);
unsigned long cur_time_sinceboot(0),prev_time(0);

volatile bool ISR_flag(false);

void IRAM_ATTR onTimer(); // pre decliariations
void clear_ISRflag();
void ISR_call(const sensors_event_t &acc,const sensors_event_t &gyro,const sensors_event_t &temp);
void test_motors(int,int); 

class Motor{
  public:
    Motor(int pinA, int pinB, int stall_speed = 52):pinA_priv(pinA), pinB_priv(pinB), stall_speed_priv(stall_speed){
    }
    void set_speed(int speed){
      speed = constrain(speed, -255, 255); //8bit PWM
      stall_speed_priv = constrain(stall_speed_priv, -255, 255);

      //there was problems using digital write so quickly after using analogue write. 
      //stall speed stops the motor burning out due EMF stress when it cant move 
      if(speed > stall_speed_priv){
        analogWrite(pinB_priv,speed);
        analogWrite(pinA_priv,LOW);
      }

      else if(speed < -stall_speed_priv)
      {
        analogWrite(pinA_priv,-speed); //this negative speed but it needs positive voltage with the pins flipped.
        analogWrite(pinB_priv,LOW);
      }

      else{
        analogWrite(pinA_priv,LOW);
        analogWrite(pinB_priv,LOW);
      }      
    }
        

  private:
    //motor pins
    int pinA_priv, pinB_priv,stall_speed_priv;
};

Adafruit_MPU6050 mpu;
sensors_event_t acc,gyro,temp; //this type is for reading values into from the sensor.
Motor motor_left(leftMtr_A, leftMtr_B); 
Motor motor_right(rightMtr_A, rightMtr_B); 



void setup() {
  Serial.begin(115200);// windows run 'mode com#' to check rate
  Wire.begin(I2C_SDA,I2C_SCL); //variables from pindef header
  
// -- setting pin modes -- //
pinMode(leftMtr_A, OUTPUT);
pinMode(leftMtr_B, OUTPUT);
pinMode(rightMtr_A, OUTPUT);
pinMode(rightMtr_B, OUTPUT);

  delay(100);
  Serial.println("MPU6050 STARTED!\n");
  
  while(!mpu.begin()){
    Serial.println("Failed to start MPU sensor! trying again...\n");
    delay(200);
  }
  
  
  Serial.println("MPU began!");
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G); //setting the +- range and sesativity. 
  mpu6050_accel_range_t acc_range = mpu.getAccelerometerRange();
  Serial.println("Gyro accelerometer range set to: ");
  Serial.println(static_cast<int>(acc_range));
  
  
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu6050_gyro_range_t gyro_range = mpu.getGyroRange();
  Serial.println("Gyro angle range set to: ");
  Serial.println(static_cast<int>(gyro_range));
  
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  mpu6050_bandwidth_t band_width = mpu.getFilterBandwidth();
  Serial.println("Gyro bandwidth set to: ");
  Serial.println(static_cast<int>(band_width));

  delay(100);

  
  // -- interupt stuff --  //
  My_timer = timerBegin(0, 80, true); //timer (id = 0, 80prescaler so it counts every 1mHz, count up = true)
  timerAttachInterrupt(My_timer, &onTimer, true);
  timerAlarmWrite(My_timer, sampletime*(1e6), true); // calls my_timer the same as sampletime but its in uS so conversion was needed, and true for resetting after its periodic. 
  timerAlarmEnable(My_timer); //starts timer
}

void loop() {
  cur_time_sinceboot = millis();
{
  if(ISR_flag){
    mpu.getEvent(&acc, &gyro, &temp); //expects pointers so put adresses.
    ISR_call(acc, gyro, temp);
    //test_motors(motorpwr,motorpwr);
    motor_left.set_speed(motorpwr);
    motor_right.set_speed(motorpwr);
    clear_ISRflag();
  }
  
  if((cur_time_sinceboot-prev_time) >= 300){ //to print the speed every 300mS for dugging
    Serial.println("Motor Power:");
    Serial.println(motorpwr);
    prev_time = cur_time_sinceboot;
}
} //so i can minimise to only see screen testing stuff


}

void test_motors(int leftspeed, int rightspeed){
    //takes left right pos or neg speeds and sets dir with the speed range = (-255,255)
  leftspeed  = constrain(leftspeed,  -255, 255);
  rightspeed = constrain(rightspeed, -255, 255);

  //there has been confusion about how this motor controller works!!  so variable names are a bit strange
  if(leftspeed > 0 ){
    digitalWrite(leftMtr_A,HIGH);
    digitalWrite(leftMtr_B,LOW);
  }
  else if(leftspeed < 0){
    digitalWrite(leftMtr_B,LOW); //positive spee in oppasite pin to go the oher way. 
    digitalWrite(leftMtr_A,HIGH);
  }

  else{
    digitalWrite(leftMtr_A,LOW);
    digitalWrite(leftMtr_B,LOW); //this makes the motors coast!
  }
  
  if(rightspeed > 0 ){
    digitalWrite(rightMtr_A,HIGH);
    digitalWrite(rightMtr_B,LOW);
  }
  else if(rightspeed < 0){
    digitalWrite(rightMtr_B,LOW); //positive spee in oppasite pin to go the oher way. 
    digitalWrite(rightMtr_A,HIGH);
  }

  else{
    digitalWrite(rightMtr_B,LOW);
    digitalWrite(rightMtr_A,LOW); //this makes the motors coast!
  }
}

//ISR to be called every 5millisecs to set flag true
void IRAM_ATTR onTimer(){
  ISR_flag = true;
}

void ISR_call(const sensors_event_t &acc,const sensors_event_t &gyro,const sensors_event_t &temp){
  
  // -- getting angle -- //
  accangle = atan2(acc.acceleration.y,acc.acceleration.z);  //inverse tan but in quadrants!
  accangle = accangle * 180/PI; //rads to degs
  
  // -- getting the gyro values -- //
  gyrorate = gyro.gyro.x * 180/PI; 
  gyroangle = gyrorate*sampletime; 
  //coverts to degrees and here is the "integration"

  //working out the error sums
  curangle = 0.9934 *(prevangle + gyroangle) + 0.0066*accangle;
  error = curangle - targ_angle;
  if (abs(motorpwr) < 255)
    errorsum = errorsum+error; //stopps integral windup
  errorsum = constrain(errorsum, -300,300);

  //calc the power to the motors from the PID constaints
  motorpwr = KP*(error) + KI*(errorsum)*sampletime + KD*(curangle - prevangle)/sampletime;
  prevangle = curangle;
}

void clear_ISRflag(){
  ISR_flag = false;
}
