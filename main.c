
/* Includes ------------------------------------------------------------------*/
#include "mbed.h"
#include "stdio.h"
#include "DevSPI.h"
#include "XNucleoIHM02A1.h"

/* Number of steps. */
#define STEPS (200 * 128)   /* 1 revolution given a 200 steps motor configured at 1/128 microstep mode. */

/* Motor Control Expansion Board. */
XNucleoIHM02A1 *x_nucleo_ihm02a1;

/* Initialization parameters of the motors connected to the expansion board. */
L6470_init_t init[L6470DAISYCHAINSIZE] = {
    /* First Motor. */
    {
        12.0,                           /* Motor supply voltage in V. */
        200,                           /* Min number of steps per revolution for the motor. */
        1.7,                           /* Max motor phase voltage in A. */
        2.55,                          /* Max motor phase voltage in V. */
        400,                           /* Motor initial speed [step/s]. */
        500.0,                         /* Motor acceleration [step/s^2] (comment for infinite acceleration mode). */
        500.0,                         /* Motor deceleration [step/s^2] (comment for infinite deceleration mode). */
        992.0,                         /* Motor maximum speed [step/s]. */
        0.0,                           /* Motor minimum speed [step/s]. */
        602.7,                         /* Motor full-step speed threshold [step/s]. */
        2.55,                          /* Holding kval [V]. */
        2.55,                          /* Constant speed kval [V]. */
        2.55,                          /* Acceleration starting kval [V]. */
        2.55,                          /* Deceleration starting kval [V]. */
        61.52,                         /* Intersect speed for bemf compensation curve slope changing [step/s]. */
        392.1569e-6,                   /* Start slope [s/step]. */
        643.1372e-6,                   /* Acceleration final slope [s/step]. */
        643.1372e-6,                   /* Deceleration final slope [s/step]. */
        0,                             /* Thermal compensation factor (range [0, 15]). */
        2.55 * 1000 * 1.10,            /* Ocd threshold [ma] (range [375 ma, 6000 ma]). */
        2.55 * 1000 * 1.00,            /* Stall threshold [ma] (range [31.25 ma, 4000 ma]). */
        StepperMotor::STEP_MODE_1_128, /* Step mode selection. */
        0xFF,                          /* Alarm conditions enable. */
        0x2E88                         /* Ic configuration. */
    },

    /* Second Motor. */
    {
        12.0,                           /* Motor supply voltage in V. */
        200,                           /* Min number of steps per revolution for the motor. */
        1.7,                           /* Max motor phase voltage in A. */
        2.55,                          /* Max motor phase voltage in V. */
        400,                           /* Motor initial speed [step/s]. */
        500.0,                         /* Motor acceleration [step/s^2] (comment for infinite acceleration mode). */
        500.0,                         /* Motor deceleration [step/s^2] (comment for infinite deceleration mode). */
        992.0,                         /* Motor maximum speed [step/s]. */
        0.0,                           /* Motor minimum speed [step/s]. */
        602.7,                         /* Motor full-step speed threshold [step/s]. */
        2.55,                          /* Holding kval [V]. */
        2.55,                          /* Constant speed kval [V]. */
        2.55,                          /* Acceleration starting kval [V]. */
        2.55,                          /* Deceleration starting kval [V]. */
        61.52,                         /* Intersect speed for bemf compensation curve slope changing [step/s]. */
        392.1569e-6,                   /* Start slope [s/step]. */
        643.1372e-6,                   /* Acceleration final slope [s/step]. */
        643.1372e-6,                   /* Deceleration final slope [s/step]. */
        0,                             /* Thermal compensation factor (range [0, 15]). */
        2.55 * 1000 * 1.10,            /* Ocd threshold [ma] (range [375 ma, 6000 ma]). */
        2.55 * 1000 * 1.00,            /* Stall threshold [ma] (range [31.25 ma, 4000 ma]). */
        StepperMotor::STEP_MODE_1_128, /* Step mode selection. */
        0xFF,                          /* Alarm conditions enable. */
        0x2E88                         /* Ic configuration. */
    }
};

DigitalOut      led(LED1);
Serial pc(SERIAL_TX, SERIAL_RX);
Thread motor_motion;
Thread serial_receive;
Thread serial_send;

typedef struct
{    
  int control;
  int type;
  int mode;
  int volume;//in uL
  int speed;//in uL/s
}mail_t;
Mail<mail_t,16> mail_box;

/*
void infuse(){
      
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->prepare_set_max_speed(800);//set the maximum speed to what the user determines
    x_nucleo_ihm02a1->perform_prepared_actions();      
    motors[0]->move(StepperMotor::BWD, STEPS*2);     
}

void refill(){
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->prepare_set_max_speed(800);//the 992 pps is not very stable
    x_nucleo_ihm02a1->perform_prepared_actions();
    motors[0]->go_home();
}
*/

void syringe_stop()
{
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->hard_stop();
}

void syringe_go(int type, int mode, double volume, double speed)
{
    L6470 **motors = x_nucleo_ihm02a1->get_components();
   
    int syringe_volume=type*10; //determine the syringe volume, type*10, in unit of mL   
    int ratio=2.77; //determine the ratio based on type(ratio: ?rev(or ?mm) = 1 mL, here we use the 30mL syringe one)
    int target_steps=volume*ratio*STEPS;//determine the steps for the motor based on ratio, volume
    int max_speed= int(ratio*200*speed);//determine the moving speed of the syringe
    motors[0]->prepare_set_max_speed(max_speed);//set the maximum speed to what the user determines
    x_nucleo_ihm02a1->perform_prepared_actions();
    //determine the direction based on mode
    if(mode==0)
    {
       motors[0]->move(StepperMotor::FWD, target_steps); 
    }
    else if(mode==1)
       motors[0]->move(StepperMotor::BWD, target_steps); 
}

//listen the command and perform tasks
void perform_task()
{
    int target_control;
    int target_type,target_mode;
    //determine the unit
    double target_volume;
    double target_speed;
    bool current_status=false;
    while(1)
    {  
        osEvent evt = mail_box.get();
        if(evt.status == osEventMail)
        {
            mail_t *mail = (mail_t*)evt.value.p;
            target_control=mail->control;
            if(target_control==0&&current_status==true)
            {
               led=0;//stop the motor
               syringe_stop();
               current_status=false;
            }
            else if(target_control==1)
            {                       
               target_type=mail->type;
               target_mode=mail->mode;
               target_volume=double(mail->volume/1000);//the volum from serial is uL, use mL in this program
               target_speed=double(mail->speed/1000);//the speed from serial is uL/s, use mL/s in this program
               led=1;//run the motor
               current_status=true;
               syringe_go(target_type, target_mode, target_volume, target_speed);            
            }
            mail_box.free(mail);
        }
    }
}

/*receive setting from the GUI*/
void serial_get()
{
    int setting[7];
    mail_t *mail = mail_box.alloc();
    while(1)
    {
        if(pc.readable())
        {                 
          for(int i=0;i<7;i++)
          {
            setting[i]=int(pc.getc());
            /*faster the stop process
            if (setting[0]==0)
            {
               mail->control=0;
               mail_box.put(mail);
               break;
            }
            */            
          } 
          if(setting[0]==1)
           {
              mail->control=1;
              mail->type=setting[1];
              mail->mode=setting[2];
              mail->volume=setting[3]*250+setting[4];
              mail->speed=setting[5]*250+setting[6];
              mail_box.put(mail);
            }
          if(setting[0]==0)     
          {
              mail->control=0;
              mail_box.put(mail);
          }  
        }                  
    }
}

/*get the number of digits, use log in the future*/
int num(int x)
{
    if(x/100>0)
      return 3;
    else if(x/10>0)
      return 2;
    else
      return 1;
    
}

/*push position data and syringe status back to the GUI*/
void serial_push()
{
    int position;
    int distance;// in mm
    int size;
    char *buf;
    char size_c[2];
    while(1)
    {
        L6470 **motors = x_nucleo_ihm02a1->get_components();
        position=motors[0]->get_position();
        distance=int(position/(200*128));
        size=num(distance);
        sprintf(size_c,"%d",size);
        buf= new char [size];//dynamically creat array
        sprintf (buf, "%d", distance);
        if(pc.writable())
            {        
                pc.putc('A');//for verify the sending
                pc.putc(size_c[0]);//sending the number of the size
                for(int i=0;i<size;i++)
                   pc.putc(buf[i]);
            }
        free(buf);
        wait(1.0);
    }
}

/* Main ----------------------------------------------------------------------*/
int main()
{
    /*----- Initialization. -----*/
    
    DevSPI dev_spi(D11, D12, D3);
    x_nucleo_ihm02a1 = new XNucleoIHM02A1(&init[0], &init[1], A4, A5, D4, A2, &dev_spi);
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    
    motors[0]->set_home();
    motors[0]->reset_device();
       
    wait(1.0);
    motor_motion.start(&perform_task);
    serial_receive.start(&serial_get);
    serial_send.start(&serial_push);
    //while(1);//this mass up the other threads
}
