
/* Includes ------------------------------------------------------------------*/

/* mbed specific header files. */
#include "mbed.h"
/* Helper header files. */
#include "DevSPI.h"
/* Expansion Board specific header files. */
#include "XNucleoIHM02A1.h"
#include "millis.h"

DigitalOut      led(LED1);
Serial pc(SERIAL_TX, SERIAL_RX);
/* Definitions ---------------------------------------------------------------*/

/* Number of steps. */
#define STEPS_1 (200 * 128)   /* 1 revolution given a 200 steps motor configured at 1/128 microstep mode. */

/* Delay in milliseconds. */
#define DELAY_1 1000
#define DELAY_2 2000
#define DELAY_3 5000



/* Variables -----------------------------------------------------------------*/

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

float b_time=0,f_time=0,r_time=0;//the variables to count the time
volatile char   c = '\0'; // Initialized the received command
/*------Parameters set up by user-----------------------------*/
//syringe volume(mL)
int syringe_c = 30;
//target volume(mL),default to 10mL
int target_c = 10;
//revolution volume ratio(revolutions per mL)
int vol_r = 4.5;
//user set speed (mL/min),default to 10mL/min
int c_speed = 10;
//custom speed (steps/s)
int m_speed;
//infusion time(s),add two more seconds to ensure the finishing of infusion
int in_time;
//steps need to reach target volume
int target_steps;
/*-------------------------------------------------------------*/
int setting;//the setting received from the interface, in integer type 
char setting_c[4];//the setting received from the interface, in character type

/*perform the infusion*/
void infuse(){  
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    /* Infuse. */
    motors[0]->prepare_set_max_speed(m_speed);//set the maximum speed to what the user determines
    x_nucleo_ihm02a1->perform_prepared_actions();
    b_time=millis();        
    motors[0]->move(StepperMotor::BWD, target_steps);
    /*waiting loop to finish the infusing*/
    while(r_time<in_time){
        f_time=millis();
        r_time=(f_time-b_time)/1000;
        wait_ms(50);
        }      
}

/*Syringe Refill*/
void refill(){
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->prepare_set_max_speed(800);//the 992 pps is not very stable
    x_nucleo_ihm02a1->perform_prepared_actions();
    motors[0]->go_home();
    motors[0]->wait_while_active();
}

/*Syringe stop*/
void stop()
{
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->hard_stop();
    wait_ms(DELAY_1);
    r_time=9999;//large number to break the waiting loop
}

/*Interrupt function to recive command*/ 
void onCharReceived()
{
    c = pc.getc();
    if(c == '0')
    {
      stop();
      c = '\0';
    }
}


/* Main ----------------------------------------------------------------------*/
int main()
{
    /*----- Initialization. -----*/   
    /* Initializing SPI bus. */
#ifdef TARGET_STM32F429
    DevSPI dev_spi(D11, D12, D13);
#else
    DevSPI dev_spi(D11, D12, D3);
#endif

    /* Initializing Motor Control Expansion Board. */
    x_nucleo_ihm02a1 = new XNucleoIHM02A1(&init[0], &init[1], A4, A5, D4, A2, &dev_spi);

    /* Building a list of motor control components. */
    L6470 **motors = x_nucleo_ihm02a1->get_components();
    motors[0]->set_home();
    millisStart();
    motors[0]->reset_device(); 
    /*------setting loop--------------*/
    int set=0;
    while(1){
       //read the setting
       if(pc.readable())
       { 
         for(int i = 0;i<4;i++)
           setting_c[i]=pc.getc();   
         setting=atoi(setting_c);
         set=1;                
        }
        //convert the setting
        target_c=setting/100;
        c_speed=setting%100;
        if(set==1){
            led=1;
            break;
        }    
    } 
    /*-----initializtion after setting---------*/
    pc.attach(&onCharReceived);
    m_speed = (200*vol_r*c_speed/60);
    in_time = (60*target_c/c_speed)+2;
    target_steps = (STEPS_1*target_c*vol_r);
    /*-----------operation loop----------------*/
    while(1){
        //response based on the received command
        if(c == '1'){
          infuse();
          wait_ms(DELAY_1);
          r_time=0;
          c = '\0'; //reset the command
        }
        if(c == '2'){
          refill();
          wait_ms(DELAY_1);
          c = '\0';        
        }       
    }
}
