/**
    @author: AMOUSSOU Z. Kenneth
    @date: 25-10-2018
    @summary: Example program to compute Eurler's angle based on MPU60x0 sensor
    @method: Kalman filter
    @chip: tested on MPU6050
    @platform: Arduino
*/
#include <MPU60x0.h>
#define DEG (180/PI)

// #####################################
//            Kalman Filter
// #####################################

class Kalman{
    public:
        Kalman();
        
        void setQAngle();
        void setQBias();
        void setR(float R);
        void setAngle(float angle);
        void setBias(float rate_bias);
        float getEstimate(float new_angle, float accel_angle, float new_rate, \
                          float sampling_time);    // estimated angle
        float getQBias();
        float getQAngle();
        float getR();
        float getRate();
        float getK();
        
        
    private:
        float Q_angle;
        float Q_bias;
        float R;
        float P[2][2];
        float K[2];
        float y;
        float rate;
        float angle;
        float bias;
};

/**
    @method: constructor
    @pamarater: none
**/
Kalman::Kalman(){
    P[0][0] = 1.0;
    P[0][1] = 0.0;
    P[1][0] = 0.0;
    P[1][1] = 1.0;

    Q_angle = 0.001;
    Q_bias = 0.003;
    R = 0.03;
    bias = 0;
    angle = 0.0;    
}

/**
    @function: getQBias
    @summary: get the bias from process covariance matrix
    @parameters: none
    @return:
        float: bias
**/
float Kalman::getQBias(){
    return Q_bias;
}

/**
    @function: getQAngle
    @summary: get the angle from process covariance matrix
    @parameters: none
    @return:
        float: angle
**/
float Kalman::getQAngle(){
    return Q_angle;
}

/**
    @function: getR
    @summary: get the measurement covariance
    @parameters: none
    @return:
        float: measurement covariance
**/
float Kalman::getR(){
    return R;
}

/**
    @function: getK
    @summary: get the kalman gain for the angle
    @parameters: none
    @return:
        float: kalman gain
**/
float Kalman::getK(){
    return K[0];
}

/**
    @function: getEstimate
    @summary: get the estimation of the angle
    @parameters:
        rate: get the new rate from the sensor
    @return:
        float: measurement covariance
**/
float Kalman::getEstimate(float new_angle, float accel_angle, float new_rate, \
                          float sampling_time){
    angle = 0.98 * (angle + (new_rate - bias) * sampling_time) + 0.02 * accel_angle;
    // compute estimation error covariance
    P[0][0] += sampling_time * (sampling_time * P[1][1] - P[0][1] - P[1][0] + Q_angle);
    P[0][1] -= sampling_time * P[1][1];
    P[1][0] -= sampling_time * P[1][1];
    P[1][1] += Q_bias * sampling_time;
    // compute Kalman gain
    float S = P[0][0] + R;
    K[0] = P[0][0] / S;
    K[1] = P[1][0] / S;
    // estimation of the angle and rate
    // sensor fusion
    y = new_angle - angle;
    angle += K[0] * y;
    bias += K[1] * y;
    // udate of the covariance error
    P[0][0] -= K[0] * P[0][0];
    P[0][1] -= K[0] * P[0][1];
    P[1][0] -= K[1] * P[0][0];
    P[1][1] -= K[1] * P[0][1];
    
    return angle;
}

/**
    @function: setBias
    @summary: set the bias for the kalman filter
    @parameters:
        rate_bias: the bias of the gyroscope
    @return: none
**/
void Kalman::setBias(float rate_bias){
    bias = rate_bias; return;
}
// #####################################

/**
    @function prototype
**/ 
void initTimer2();


MPU60x0 mySensor;

Kalman rollEstimator;
Kalman pitchEstimator;
Kalman yawEstimator;

/** Data structure

    struct{
        float accelX = 0;
        float accelY = 0;
        float accelZ = 0;
        
        float temp = 0;
        
        float gyroX = 0;
        float gyroY = 0;
        float gyroZ = 0;
    } IMU_DATA 
*/
IMU_DATA data;

/**
    Variables
*/
float yaw = 0.0;
float pitch = 0.0;
float roll = 0.0;

float accel_pitch = 0.0;
float accel_roll = 0.0;

float theta = 0.0;

// timer 2 interrupt notification flag in software
volatile bool flag = false;

/*
 Sampling time in second (s)
 This is calculate out of the timer 2 interrupt frequency
     Fosc = 16MHz
     prescaler = 1024
     47 steps  -> 3.008ms (~ 332.4468085 Hz) **
     TCNT2 = (255 - 47)
     sampling time = 3.008ms = 0.003008s
*/ 
const float SAMPLING_TIME = 0.003008;

/* get gyroscope offset */
float gyro_x_offset = 0.0;
float gyro_y_offset = 0.0;
float gyro_z_offset = 0.0;

unsigned int i = 0;
volatile uint8_t counter = 0; // used to delay the display rate of angles

void setup(){
   /* Initialize serial communication */
   Serial.begin(9600);
  
  /* Initialize sensor for measurement 
     Disable sleep mode + set up full scale range
  */
  mySensor.begin();
  
  /* Check correct wiring of the sensor */
  while(mySensor.whoami() != ADDR){
    Serial.println("MPU60x0 not found!");
    delay(200);     // 200ms
  }
  /* Read device I2C adress */
  Serial.print("Sensor ADDR: 0x");
  Serial.println(mySensor.whoami(), HEX);
  
  /* get Gyroscope's offset */
  for(i = 0; i <= 200; i++){
    data = mySensor.read();
    gyro_x_offset += data.gyroX;
    gyro_y_offset += data.gyroY;
    gyro_z_offset += data.gyroZ;
    // delay of 3ms
    // same value as the sampling time
    delay(3);   // 3 ms
  }
  gyro_x_offset /= i;
  gyro_y_offset /= i;
  gyro_z_offset /= i;
  
  // estimator initialization
  rollEstimator.setBias(gyro_x_offset);
  pitchEstimator.setBias(gyro_y_offset);
  yawEstimator.setBias(gyro_z_offset);
  
  // initer timer
  initTimer2();
}

void loop(){
    if(flag){
        data = mySensor.read();     // read new data
        
        // Process new data to get Euler's angles
        accel_roll = (float)atan2(-data.accelY, data.accelZ);
        accel_roll *= DEG; // convert roll angle in degree
        
        accel_pitch = (float)atan2(data.accelX, sqrt(data.accelY * data.accelY \
                                    + data.accelZ * data.accelZ));
        accel_pitch *= DEG; // convert pitch angle in degree
        // Kalman filter
        roll = rollEstimator.getEstimate(roll, accel_roll, data.gyroX, SAMPLING_TIME);
        pitch = pitchEstimator.getEstimate(pitch, accel_pitch, data.gyroY, SAMPLING_TIME);
        yaw = yawEstimator.getEstimate(yaw, 0.0, data.gyroZ, SAMPLING_TIME);
        
        // compute complementary filter on roll angle for comparison purpose
        theta = 0.98 * (theta + data.gyroX * SAMPLING_TIME) + 0.02 * accel_roll;
        
        // interrupt flag
        flag = false;
    }
    
    // print out angles
    if(counter >= 100){   // every ~300ms
        /* Serial.println("Yaw: " + String(yaw) + "°");
        Serial.println("Pitch: " + String(pitch) + "°");
        Serial.println("Roll: " + String(roll) + "°"); */
        Serial.print(String(roll));
        Serial.print(",");
        Serial.print(String(theta));
        Serial.println();
        counter = 0;
    }
}

/**
    @function: initTimer2
    @summary: configure the timer 2 module of the ATMEGA328 µC
    @parameters: none
    @return: none
**/
void initTimer2(){
    cli();
    TCCR2A = 0x00;
    ASSR &= ~(1<<5); // clk_io
    /**
        Fosc = 16MHz
        prescaler = 1024
        256 steps -> 16.384ms
        32 steps  -> 2.048ms (~ 488.28125 Hz)
        47 steps  -> 3.008ms (~ 332.4468085 Hz) **
        63 steps  -> 4.032ms (~ 248.015873 Hz) 
    **/
    TCNT2 = (255 - 47);
    GTCCR &= ~(1<<7);   // disable timer 2 prescaler reset
    TCCR2B = 0x07;
    TIFR2 = 0x00;
    TIMSK2 = 0x01;
    sei();
}

/**
    Interrupt Service Routine
    @vector: timer 2 vector
**/
SIGNAL(TIMER2_OVF_vect){
    cli();                      // disable interrupt
    TIFR2 = 0x00;               // clear interrupt flag
    flag = true;
    counter++;
    TCNT2 = (255 - 47);         // reset timer2 counter register
    sei();                      // enable interrupt
}
