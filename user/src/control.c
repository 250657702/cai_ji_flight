#include "control.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "time.h"

#include "sbus.h"
#include "mpu6050.h"
#include "bmp280.h"
#include "spl06.h"
#include "pwm.h"

#include "quaternion.h"
#include "remote.h"
#include "pid.h"
#include "common.h"
#include "filter.h"




PID_t pitch_rate_pid;
PID_t pitch_angle_pid;

PID_t roll_rate_pid;
PID_t roll_angle_pid;

PID_t yaw_rate_pid;
PID_t yaw_angle_pid;


PID_t velocity_z_pid;
PID_t position_z_pid;

biquadFilter_t pitch_d_ltem_biquad_parameter = {0};
biquadFilter_t roll_d_ltem_biquad_parameter = {0};
biquadFilter_t yaw_d_ltem_biquad_parameter = {0};

biquadFilter_t position_z_d_ltem_biquad_parameter = {0};

#define ALTITUDE_SLIDING_FILTER_SIZE    5
float altitude_sliding_filter_buff[ALTITUDE_SLIDING_FILTER_SIZE] = {0};


#define GRAVITY_ACC         980

Triaxial_Data_t earth_acc = {0};

Triaxial_Data_t estimate_velocity = {0};
Triaxial_Data_t estimate_position = {0};
Triaxial_Data_t estimate_acc = {0};

Triaxial_Data_t Origion_NamelessQuad_velocity = {0};
Triaxial_Data_t Origion_NamelessQuad_position = {0};
Triaxial_Data_t Origion_NamelessQuad_acc = {0};


float baro_compensate_k = 0.02;





void pid_init()
{
	pitch_rate_pid.p = 3.6;
	pitch_rate_pid.i = 0.02;
	pitch_rate_pid.d = 52;    //18
	pitch_rate_pid.i_sum = 0;
	pitch_rate_pid.last_error = 0;
	pitch_rate_pid.i_sum_min = -500;
	pitch_rate_pid.i_sum_max = 500;
	pitch_rate_pid.out_min = -ATTITUDE_CONTROL_OUT_MAX;
	pitch_rate_pid.out_max = ATTITUDE_CONTROL_OUT_MAX;
	
	
	pitch_angle_pid.p = 7;       
	pitch_angle_pid.i = 0.0;
	pitch_angle_pid.d = 0.0;     
	pitch_angle_pid.i_sum = 0;
	pitch_angle_pid.last_error = 0;
	pitch_angle_pid.i_sum_min = -0;
	pitch_angle_pid.i_sum_max = 0;
	pitch_angle_pid.out_min = -300;
	pitch_angle_pid.out_max = 300;
	
	roll_rate_pid.p = pitch_rate_pid.p * 0.75;
	roll_rate_pid.i = pitch_rate_pid.i * 0.75;
	roll_rate_pid.d = pitch_rate_pid.d * 0.75;
	roll_rate_pid.i_sum = pitch_rate_pid.i_sum;
	roll_rate_pid.last_error = pitch_rate_pid.last_error;
	roll_rate_pid.i_sum_min = pitch_rate_pid.i_sum_min;
	roll_rate_pid.i_sum_max = pitch_rate_pid.i_sum_max;
	roll_rate_pid.out_min = pitch_rate_pid.out_min;
	roll_rate_pid.out_max = pitch_rate_pid.out_max;
	
	roll_angle_pid.p = pitch_angle_pid.p;
	roll_angle_pid.i = pitch_angle_pid.i;
	roll_angle_pid.d = pitch_angle_pid.d;
	roll_angle_pid.i_sum = pitch_angle_pid.i_sum;
	roll_angle_pid.last_error = pitch_angle_pid.last_error;
	roll_angle_pid.i_sum_min = pitch_angle_pid.i_sum_min;
	roll_angle_pid.i_sum_max = pitch_angle_pid.i_sum_max;
	roll_angle_pid.out_min = pitch_angle_pid.out_min;
	roll_angle_pid.out_max = pitch_angle_pid.out_max;
	
	yaw_rate_pid.p = 8;
	yaw_rate_pid.i = 0.010;
	yaw_rate_pid.d = 0;
	yaw_rate_pid.i_sum = 0;
	yaw_rate_pid.last_error = 0;
	yaw_rate_pid.i_sum_min = pitch_rate_pid.i_sum_min;
	yaw_rate_pid.i_sum_max = pitch_rate_pid.i_sum_max;
	yaw_rate_pid.out_min = pitch_rate_pid.out_min;
	yaw_rate_pid.out_max = pitch_rate_pid.out_max;
	
	yaw_angle_pid.p = pitch_angle_pid.p;
	yaw_angle_pid.i = pitch_angle_pid.i;
	yaw_angle_pid.d = pitch_angle_pid.d;
	yaw_angle_pid.i_sum = pitch_angle_pid.i_sum;
	yaw_angle_pid.last_error = pitch_angle_pid.last_error;
	yaw_angle_pid.i_sum_min = pitch_angle_pid.i_sum_min;
	yaw_angle_pid.i_sum_max = pitch_angle_pid.i_sum_max;
	yaw_angle_pid.out_min = pitch_angle_pid.out_min;
	yaw_angle_pid.out_max = pitch_angle_pid.out_max;

	velocity_z_pid.p = 400;   //150
	velocity_z_pid.i = 3;   //1
	velocity_z_pid.d = 200; //200
	velocity_z_pid.i_sum = 0;
	velocity_z_pid.last_error = 0;
	velocity_z_pid.i_sum_min = -500;
	velocity_z_pid.i_sum_max = 500;
	velocity_z_pid.out_min = -ALTITUDE_THROTTLE_CONTROL_OUT_MAX;
	velocity_z_pid.out_max = ALTITUDE_THROTTLE_CONTROL_OUT_MAX;

	position_z_pid.p = 0.45;
	position_z_pid.i = 0;
	position_z_pid.d = 0;
	position_z_pid.i_sum = 0;
	position_z_pid.last_error = 0;
	position_z_pid.i_sum_min = -0.3;
	position_z_pid.i_sum_max = 0.3;
	position_z_pid.out_min = -1;
	position_z_pid.out_max = 1;


	biquad_filter_init_lpf(&pitch_d_ltem_biquad_parameter, 500, 80);
	biquad_filter_init_lpf(&roll_d_ltem_biquad_parameter, 500, 80);
	biquad_filter_init_lpf(&yaw_d_ltem_biquad_parameter, 500, 80);

	biquad_filter_init_lpf(&position_z_d_ltem_biquad_parameter, 500, 50);

}


void motor_stop()
{
	PWM_Channel_Type pwm_channel;
	for(pwm_channel = PWM_Channel_1;pwm_channel < PWM_Channel_MAX;pwm_channel++)
	{
		pwm_out(pwm_channel, MOTOR_STOP_PWM);
	}
}



void attitude_control(uint32_t throttle_out,float set_pitch,float set_roll,float set_yaw)
{
	static uint8_t attitude_control_count = 0;

	PWM_Channel_Type pwm_channel;

	float yaw_error;

	static float  pitch_rate_out = 0;
	int32_t pitch_out = 0;

	static float roll_rate_out = 0;
	int32_t roll_out = 0;

	static float yaw_rate_out = 0;
	int32_t yaw_out = 0;
	

	int32_t motor_pwm_out[4];

	yaw_error = set_yaw - Angle.yaw;
	if(yaw_error > 180)
	{
		yaw_error -= 360;
	}
	else if(yaw_error < -180)
	{
		yaw_error += 360;
	}
	
	set_pitch += PITCH_ZERO;
	set_roll += ROLL_ZERO;


	attitude_control_count++;
	if(1 == attitude_control_count)
	{
		pitch_rate_out = PID_control(&pitch_angle_pid, Angle.pitch - set_pitch);
	}
	else if(2 == attitude_control_count)
	{
		roll_rate_out = PID_control(&roll_angle_pid,Angle.roll - set_roll);
	}
	else if(3 == attitude_control_count)
	{
		attitude_control_count = 0;
		yaw_rate_out = PID_control(&yaw_angle_pid,yaw_error); 
	}

	pitch_out = (int32_t)PID_control_biquad(&pitch_rate_pid, -Gyro.y - pitch_rate_out, &pitch_d_ltem_biquad_parameter);
	roll_out = (int32_t)PID_control_biquad(&roll_rate_pid, -Gyro.x - roll_rate_out, &roll_d_ltem_biquad_parameter);
	yaw_out = (int32_t)PID_control_biquad(&yaw_rate_pid, Gyro.z - yaw_rate_out, &yaw_d_ltem_biquad_parameter);

	
	motor_pwm_out[0] = MOTOR_PWM_MIN + pitch_out - roll_out + yaw_out + throttle_out;  //400
	motor_pwm_out[1] = MOTOR_PWM_MIN + pitch_out + roll_out - yaw_out + throttle_out;     
	motor_pwm_out[2] = MOTOR_PWM_MIN - pitch_out - roll_out - yaw_out + throttle_out;   //400
	motor_pwm_out[3] = MOTOR_PWM_MIN - pitch_out + roll_out + yaw_out + throttle_out;
	
	for(pwm_channel = PWM_Channel_1;pwm_channel < PWM_Channel_MAX;pwm_channel++)
	{
		motor_pwm_out[pwm_channel] = int_range(motor_pwm_out[pwm_channel],MOTOR_PWM_MIN,MOTOR_PWM_MAX);
		pwm_out(pwm_channel, motor_pwm_out[pwm_channel]);
	}

	
}


#define TIME_CONTANST_Z     5.0f
#define K_ACC_Z (5.0f / (TIME_CONTANST_Z * TIME_CONTANST_Z * TIME_CONTANST_Z))
#define K_VEL_Z (3.0f / (TIME_CONTANST_Z * TIME_CONTANST_Z))
#define K_POS_Z (3.0f / TIME_CONTANST_Z)

float  High_Filter[3]={
	0.015,  //0.03
	0.05,//0.05
	0.02//0.03
};


float Altitude_Dealt=0;

float acc_correction[3] = {0};
float vel_correction[3] = {0};
float pos_correction[3] = {0};

float SpeedDealt[3] = {0};

 

void Strapdown_INS_High()
{
	Triaxial_Data_t acc;
	float dt = 0.002f;

	acc.x = Origion_NamelessQuad_acc.x * GRAVITY_ACC;
	acc.y = Origion_NamelessQuad_acc.y * GRAVITY_ACC;
	acc.z = Origion_NamelessQuad_acc.z * GRAVITY_ACC;

	imu_body_to_Earth(&acc,&earth_acc);
	earth_acc.z -= GRAVITY_ACC;

	Altitude_Dealt=bmp280_data.altitude - estimate_position.z;//气压计(超声波)与SINS估计量的差，单位cm

	acc_correction[2] += High_Filter[0]*Altitude_Dealt* K_ACC_Z ;//加速度校正量
	vel_correction[2] += High_Filter[1]*Altitude_Dealt* K_VEL_Z ;//速度校正量
	pos_correction[2] += High_Filter[2]*Altitude_Dealt* K_POS_Z ;//位置校正量

	//原始加速度+加速度校正量=融合后的加速度
	estimate_acc.z = earth_acc.z + acc_correction[2];

	//融合后的加速度积分得到速度增量
	SpeedDealt[2]=estimate_acc.z * dt;

	//得到速度增量后，更新原始位置
	Origion_NamelessQuad_position.z += (estimate_velocity.z + 0.5 * SpeedDealt[2]) * dt;

	//原始位置+位置校正量=融合后位置
	estimate_position.z = Origion_NamelessQuad_position.z + pos_correction[2];

	//原始速度+速度校正量=融合后的速度
	Origion_NamelessQuad_velocity.z += SpeedDealt[2];
	estimate_velocity.z = Origion_NamelessQuad_velocity.z + vel_correction[2];
}



float altitude_error;
void get_estimate_position(float observe_altitude)
{
	Triaxial_Data_t acc;
	float dt = 0.002;

//float altitude_error;

	acc.x = Origion_NamelessQuad_acc.x * GRAVITY_ACC;
	acc.y = Origion_NamelessQuad_acc.y * GRAVITY_ACC;
	acc.z = Origion_NamelessQuad_acc.z * GRAVITY_ACC;

	imu_body_to_Earth(&acc,&earth_acc);
	earth_acc.z -= GRAVITY_ACC + 40;

	altitude_error = observe_altitude - estimate_position.z;
	
	//偏差过大 直接给值
	if(ABS(altitude_error) > 500)
	{
		estimate_position.z = observe_altitude;
		estimate_velocity.z = 0;
	}
	else
	{
		estimate_velocity.z += earth_acc.z * dt;
		estimate_position.z += estimate_velocity.z * dt + 0.5*earth_acc.z*dt*dt;
		

		estimate_position.z += baro_compensate_k * altitude_error;
		estimate_velocity.z += baro_compensate_k  * altitude_error;
	}

}


//位置控制
uint32_t position_control(float now_altitude, float set_altitude)
{
	uint8_t count = 0;
	int32_t throttle_out = 0; 
	static float set_velocity_z = 0;
	
	count++;
	if(count >= 5)
	{
		count = 0;
		set_velocity_z = PID_control(&position_z_pid, now_altitude - set_altitude);
		
	}
	throttle_out = (int32_t)PID_control_biquad(&velocity_z_pid, set_velocity_z - estimate_velocity.z, &position_z_d_ltem_biquad_parameter);
	throttle_out += THROTTLE_BASE;

	if(throttle_out < 0)
	{
		throttle_out = 0;
	}

	return throttle_out;
}



uint32_t code_time = 0;

void control_task(void * parameters)
{
	extern xQueueHandle remote_queue;

	extern xQueueHandle printf_velocity_queue;
	extern xQueueHandle printf_position_queue;
	extern xQueueHandle printf_acc_queue;

	TickType_t time = xTaskGetTickCount();
	uint32_t time_count = 0;
	

	//uint16_t remote_data[IBUS_Channel_MAX] = {0};
	remote_data_t remote_data;
	int32_t remote_lose_count = 0;
	uint8_t remote_lose_flag = 0;

	float set_yaw = 0;
	float set_altitude = 0;
	flight_mode_t last_remote_mode = Stabilize_Mode;
	

	for(;;)
	{
		time_count++;
		if(time_count >= 1000)
		{
			time_count = 0;
		}
		//time_count_start_us();

		//接收遥控器数据
		if(xQueueReceive(remote_queue,&remote_data,0) == pdFALSE)
		{
			remote_lose_count++;
			if(remote_lose_count > 100)
			{
				remote_lose_count = 0;
				remote_lose_flag = 1;
			}
		}
		else
		{
			remote_lose_count = 0;
			remote_lose_flag = 0;
		}

		if((time_count % 20) == 0)
		{
			//接收bmp180海拔高度 
			//bmp280_data_get(&bmp280_data);
			//bmp280_data.altitude = sliding_filter(bmp280_data.altitude,altitude_sliding_filter_buff,ALTITUDE_SLIDING_FILTER_SIZE);
			
			spl06_get_all_data();
		}
		
		//获取mpu6050原始数据
		mpu6050_get_gyro(&Mpu6050_Gyro);
        mpu6050_get_acc(&Mpu6050_Acc);
		//单位转换并滤波
	    gyro_data_change();
		Origion_NamelessQuad_acc.x = Acc.x;
		Origion_NamelessQuad_acc.y = Acc.y;
		Origion_NamelessQuad_acc.z = Acc.z;
		//四元数姿态解算
        imuUpdate(&Acc, &Gyro, &Angle , 0.002);

		//预估机体速度和位置
		//get_estimate_position(bmp280_data.altitude);
		//bmp280_data.altitude = 0;
		Strapdown_INS_High();
		
		//需要打印的位置信息
		//xQueueSend(printf_position_queue,&bmp280_data.altitude,0);
		//xQueueSend(printf_position_queue,&bmp280_data.pressure,0);
		xQueueSend(printf_position_queue,&estimate_position.z,0);

		//需要打印的速度信息
		xQueueSend(printf_velocity_queue,&estimate_velocity.z,0);
		
		//需要打印的加速度信息
		xQueueSend(printf_acc_queue,&estimate_acc.z,0);
		//xQueueSend(printf_acc_queue,&earth_acc.z,0);
		//xQueueSend(printf_acc_queue,&Origion_NamelessQuad_acc.z,0);
		
		
		if((remote_data.throttle_out < THROTTLE_DEAD_ZONE) || (1 == remote_lose_flag))
		{
			//电机停转
			motor_stop();
			//清除PID角速度环I项
			PID_i_sum_clean(&pitch_rate_pid);
			PID_i_sum_clean(&roll_rate_pid);
			PID_i_sum_clean(&yaw_rate_pid);

			//将设定偏航角设置为当前偏航角，防止起飞旋转
			set_yaw = Angle.yaw;

		}
		else
		{
			set_yaw += remote_data.set_yaw_rate;
			if(set_yaw > 180)
			{
				set_yaw -= 360;
			}
			else if(set_yaw < -180)
			{
				set_yaw += 360;
			}

			if(remote_data.mode == Auto_Mode)
			{
				if(last_remote_mode != Auto_Mode)
				{
					set_altitude = estimate_position.z;
				}
				remote_data.throttle_out = position_control(estimate_position.z, set_altitude);
			}
			last_remote_mode = remote_data.mode;

			
			attitude_control(remote_data.throttle_out,remote_data.set_pitch,remote_data.set_roll,set_yaw);
		}
		//code_time = time_count_end_us();
		
		vTaskDelayUntil(&time,2);
	}
}
