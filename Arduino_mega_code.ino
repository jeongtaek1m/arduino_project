#define ENC_COUNT_REV 330 //pulse per 1 rotation(360degree)
#define WHEEL_RADIUS 0.067

//Left Motor
#define Encoder_L_PulseA 18 // Yellow
#define Encoder_L_PulseB 19 // White
#define EN_L 9 //left velocity
#define Motor_L_pin1 2
#define Motor_L_pin2 3 //left motor 

//Right Motor
#define Encoder_R_PulseA 20 // Yellow
#define Encoder_R_PulseB 21 // White
#define Motor_R_pin1 4 
#define Motor_R_pin2 5 //right motor
#define EN_R 10 //right velocity

#define pwm_L 255

//MPU sensor
#define MPU6050_ADDRESS 0x68
#define Accel 0x3B
#define Gyro 0x43

// MPU6050의 스케일 설정
const float ACCEL_SCALE = 16384.0; // 16384 LSB/g
const float GYRO_SCALE = 131.0;    // 131 LSB/(º/s)
const float dt_ = 0.01;
float accel_x, accel_y, accel_z;
float gyro_x, gyro_y, gyro_z;
float velocity_x = 0.0;
float velocity_y = 0.0;
float displacement_x = 0.0;
float displacement_y = 0.0;
float angleZ = 0.0;
int crash = 99999999;

//relay_channel
#define relay1 PB6 //digital pin 12
#define relay2 PB7 //digital pin 13

volatile int EncoderCount_L = 0; //per one rotation
float kp, kd, ki;
double integral = 0.0;
long preT = 0;
double error=0.0;
double pre_error=0.0;

void setup() {
  Serial.begin(115200);
  //left motor pinmode
  pinMode(Motor_L_pin1, OUTPUT);
  pinMode(Motor_L_pin2, OUTPUT);
  pinMode(EN_L,  OUTPUT);
  pinMode(Encoder_L_PulseA, INPUT);
  pinMode(Encoder_L_PulseB, INPUT);

  //right motor pinmode
  pinMode(Motor_R_pin1, OUTPUT);
  pinMode(Motor_R_pin2, OUTPUT);
  pinMode(EN_R,  OUTPUT);
  pinMode(Encoder_R_PulseA, INPUT_PULLUP);
  pinMode(Encoder_R_PulseB, INPUT_PULLUP);
  delay(5000); // delay 5 seconds to run

  attachInterrupt(digitalPinToInterrupt(Encoder_L_PulseA),EncoderPositionRead, RISING);

  digitalWrite(Motor_L_pin1, HIGH);
  digitalWrite(Motor_L_pin2, LOW); //확인해보기 앞 뒤 주행
  analogWrite(EN_L, pwm_L);
  PID_gain();

  writeRegister(MPU6050_ADDRESS, 0x6B, 0);

  //pinMode(relay1, OUTPUT);
  //pinMode(relay2, OUTPUT);
  DDRB |= (1<<relay1)|(1<<relay2);
}

void loop() {
  long nowT = micros();
  double dt = ((double)(nowT - preT)/1.0e6);
  float velocity_L = Convert_CtoV(EncoderCount_L, dt);

  //pid
  pid(error, dt);

  //for next loop
  EncoderCount_L = 0;
  preT = nowT;

  //mpu6050
  mpu();

  //relay channel switching
  relay_channel_on();
  relay_channel_off();
}

//반시계 방향이 정방향일 때 (Left Motor 기준)
void EncoderPositionRead() {
  if (digitalRead(Encoder_L_PulseA) == digitalRead(Encoder_L_PulseB)) {
    EncoderCount_L++;
  } 
  else {
    EncoderCount_L--;
  }
}

void PID_gain(){
  kp = 0.8;
  ki = 0.2;
  kd = 0.1;
}

double pid(double error, double dt)
{
  double proportional = error;
  integral = integral + error * dt;
  double derivative = (error - pre_error) / dt;
  pre_error = error;
  double output = (kp * proportional) + (ki * integral) + (kd * derivative);
  return output;
}

float Convert_CtoV(int count, double dt){
  float rpm = (float)(count * 60.0 / ENC_COUNT_REV /dt);
  float circumference = 2 * 3.141592 * WHEEL_RADIUS;
  float velocity = rpm / 60.0 * circumference;
  return velocity;
}

void mpu() {
  readAccelData();
  readGyroData();
  
  calculateDirection();
  calculateDisplacement();

  if (accel_x^2+accel_y^2+accel_z^2 > crash){
    relay_channel_off();
  }

  Serial.print(displacement_x);
  Serial.print(" ");
  Serial.print(displacement_y);
  Serial.print(" ");
  Serial.print(angleZ);
  Serial.print(" ");

  Serial.print(accel_x);
  Serial.print(",");
  Serial.print(accel_y);
  Serial.print(",");
  Serial.print(accel_z);
  Serial.print(",");

  Serial.print(gyro_x);
  Serial.print(",");
  Serial.print(gyro_y);
  Serial.print(",");
  Serial.println(gyro_z);

  delay(10);
}

void writeRegister(uint8_t address, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void readRegisters(uint8_t address, uint8_t reg, uint8_t count, uint8_t* dest) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(address, count, true);
  for (int i = 0; i < count; i++) {
    dest[i] = Wire.read();
  }
}

void readAccelData() {
  uint8_t rawData[6];
  readRegisters(MPU6050_ADDRESS, Accel, 6, rawData);

  accel_x = ((int16_t)(rawData[0] << 8 | rawData[1])) / ACCEL_SCALE + 0.002;
  accel_y = ((int16_t)(rawData[2] << 8 | rawData[3])) / ACCEL_SCALE - 0.008;
  accel_z = ((int16_t)(rawData[4] << 8 | rawData[5])) / ACCEL_SCALE + 0.020;
}

void readGyroData() {
  uint8_t rawData[6];
  readRegisters(MPU6050_ADDRESS, Gyro, 6, rawData);

  gyro_x = ((int16_t)(rawData[0] << 8 | rawData[1])) / GYRO_SCALE * (PI / 180.0);
  gyro_y = ((int16_t)(rawData[2] << 8 | rawData[3])) / GYRO_SCALE * (PI / 180.0);
  gyro_z = ((int16_t)(rawData[4] << 8 | rawData[5])) / GYRO_SCALE * (PI / 180.0);
}

void calculateDisplacement() {
  // 속도 적분
  velocity_x += accel_x * dt;
  velocity_y += accel_y * dt;

  // 변위 적분
  displacement_x += velocity_x * dt;
  displacement_y += velocity_y * dt;
}

void calculateDirection() {
  angleZ += gyro_z * dt;
}

void relay_channel_on() 
{
   //digitalWrite(relay1, HIGH);
   //digitalWrite(relay2, HIGH);
  // PORTB |= (1<<relay1)|(1<<relay2);
  PORTB |= (1<<relay1);
  PORTB |= (1<<relay2); //동시진행 안한 이유 회로의 안전성 때문
}

void relay_channel_off() 
{
   //digitalWrite(relay1, LOW);
   //digitalWrite(relay2, LOW);
  //PORTB &= ~((1<<relay1)|(1<<relay2))
  PORTB &= ~((1<<relay1));
  PORTB &= ~((1<<relay2)); //동시진행 안한 이유 회로의 안전성 때문
}
